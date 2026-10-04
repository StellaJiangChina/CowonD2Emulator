/***************************************************************************
 * NAND raw dumper — LOW half for Cowon D2
 * Auto-detects NAND chip ID and page size at runtime.
 *
 * Drop into apps/plugins/, add "nand_dump_lo.c" to apps/plugins/SOURCES.
 ****************************************************************************/

#include "plugin.h"

/* ---- TCC7801 NAND registers come from tcc780x.h (via plugin.h) ---- */
#define WE_GPIO_BIT (1<<19)
#define CS_GPIO_BIT (1<<21)

/* ---- which half to dump ---- */
#ifndef DUMP_HALF
#define DUMP_HALF 0
#endif

#if DUMP_HALF
#define DUMP_FILE "/nand_dump_hi.bin"
#else
#define DUMP_FILE "/nand_dump_lo.bin"
#endif

/* ---- NAND chip table (from firmware/drivers/nand_id.c) ---- */
struct nand_chip {
    unsigned char id1, id2;
    int pages_per_block;
    int blocks_per_bank;
    int page_size;
    int spare_size;
    int col_cycles;
    int row_cycles;
    int planes;
};

static const struct nand_chip chip_table[] = {
    {0xDC, 0x10,  64, 4096, 2048,  64, 2, 3, 1},  /* K9F4G08U0M */
    {0xD3, 0x51,  64, 8192, 2048,  64, 2, 3, 1},  /* K9K8G08U0M */
    {0xD5, 0x14, 128, 4096, 4096, 128, 2, 3, 2},  /* K9GAG08U0M */
    {0xD5, 0x55, 128, 8192, 2048,  64, 2, 3, 4},  /* K9LAG08U0M */
    {0xD7, 0x55, 128, 8192, 4096, 128, 2, 3, 4},  /* K9LBG08U0M */
};
#define NUM_CHIPS (sizeof(chip_table)/sizeof(chip_table[0]))

/* runtime-detected parameters */
static int g_page_size, g_spare_size, g_col_cycles, g_row_cycles;
static long g_rows_per_bank;
static int g_bytes_per_page;

/* ---- low-level helpers ---- */
static void chip_select(int bank)
{
    if (bank < 0) {
        GPIOB_CLEAR = CS_GPIO_BIT;
        NFC_CTRL |= NFC_CS0 | NFC_CS1;
    } else {
        if (bank & 1) { NFC_CTRL &= ~NFC_CS0; NFC_CTRL |=  NFC_CS1; }
        else          { NFC_CTRL |=  NFC_CS0; NFC_CTRL &= ~NFC_CS1; }
        if (bank & 2) GPIOB_SET   = CS_GPIO_BIT;
        else          GPIOB_CLEAR = CS_GPIO_BIT;
    }
}

static void read_raw_page(int row, unsigned char *buf)
{
    int i;
    BCLKCTR |= DEV_NAND;
    NFC_CTRL = (NFC_CTRL & ~0xFFF) | 0x131;
    chip_select(0);
    GPIOB_CLEAR = WE_GPIO_BIT;
    NFC_CTRL &= ~NFC_16BIT;

    NFC_CMD = 0x00;
    for (i = 0; i < g_col_cycles; i++) NFC_SADDR = 0x00;
    for (i = 0; i < g_row_cycles; i++) { NFC_SADDR = row & 0xFF; row >>= 8; }
    NFC_CMD = 0x30;
    while (!(NFC_CTRL & NFC_READY)) ;

    for (i = 0; i < (g_bytes_per_page / 4); i++)
        ((unsigned int *)buf)[i] = NFC_WDATA;

    chip_select(-1);
    BCLKCTR &= ~DEV_NAND;
}

/* ---- plugin entry ---- */
enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    unsigned char id[5];
    const struct nand_chip *chip = NULL;
    long row, row_start, row_end, total_pages, done;
    int fd, i, pages_per_buf;
    unsigned char *buf;
    size_t buf_size;

    /* ---- read NAND ID ---- */
    BCLKCTR |= DEV_NAND;
    NFC_RST  = 0;
    NFC_CTRL = (NFC_CTRL & ~0xFFF) | 0x353;
    chip_select(0);
    GPIOB_CLEAR = WE_GPIO_BIT;
    NFC_CTRL &= ~NFC_16BIT;
    NFC_CMD = 0xFF;
    NFC_CMD = 0x90;
    NFC_SADDR = 0x00;
    id[0] = NFC_SDATA; id[1] = NFC_SDATA; id[2] = NFC_SDATA;
    id[3] = NFC_SDATA; id[4] = NFC_SDATA;
    chip_select(-1);
    BCLKCTR &= ~DEV_NAND;

    /* match chip */
    for (i = 0; i < NUM_CHIPS; i++) {
        if (chip_table[i].id1 == id[1] && chip_table[i].id2 == id[2]) {
            chip = &chip_table[i];
            break;
        }
    }
    if (!chip) {
        char msg[48];
        rb->snprintf(msg, sizeof(msg), "Unknown NAND: %02x %02x %02x",
                     id[0], id[1], id[2]);
        rb->splash(HZ*3, msg);
        return PLUGIN_ERROR;
    }

    g_page_size     = chip->page_size;
    g_spare_size    = chip->spare_size;
    g_col_cycles    = chip->col_cycles;
    g_row_cycles    = chip->row_cycles;
    g_bytes_per_page = g_page_size + g_spare_size;
    g_rows_per_bank  = (long)chip->blocks_per_bank * chip->pages_per_block;

    total_pages = g_rows_per_bank / 2;
#if DUMP_HALF
    row_start = g_rows_per_bank / 2;
    row_end   = g_rows_per_bank;
#else
    row_start = 0;
    row_end   = g_rows_per_bank / 2;
#endif

    /* show chip info */
    {
        char msg[64];
        rb->snprintf(msg, sizeof(msg),
                     "NAND %02x%02x: %d+%d B/page, %ld rows",
                     id[1], id[2], g_page_size, g_spare_size, g_rows_per_bank);
        rb->splash(HZ*2, msg);
    }

    buf = rb->plugin_get_audio_buffer(&buf_size);
    if (!buf || buf_size < (size_t)g_bytes_per_page) {
        rb->splash(HZ*2, "No audio buffer");
        return PLUGIN_ERROR;
    }
    pages_per_buf = buf_size / g_bytes_per_page;

    fd = rb->open(DUMP_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        rb->splash(HZ*3, "Cannot open " DUMP_FILE);
        return PLUGIN_ERROR;
    }

    rb->lcd_clear_display();
    done = 0;

    for (row = row_start; row < row_end; row++) {
        int idx = done % pages_per_buf;
        read_raw_page((int)row, buf + (size_t)idx * g_bytes_per_page);
        done++;

        if (idx == pages_per_buf - 1 || done == total_pages)
            rb->write(fd, buf, (size_t)(idx + 1) * g_bytes_per_page);

        if ((done & 0x3F) == 0) {
            char l1[32], l2[40];
            int pct = (int)(done * 100 / total_pages);
            rb->snprintf(l1, sizeof(l1), DUMP_HALF ? "Dumping HI half" : "Dumping LO half");
            rb->snprintf(l2, sizeof(l2), "row %ld  %d%%", row, pct);
            rb->lcd_puts_scroll(0, 0, l1);
            rb->lcd_puts_scroll(0, 1, l2);
            rb->lcd_update();
            rb->yield();
            /* only POWER button (0x01 on Cowon D2) aborts */
            if (rb->button_get_w_tmo(0) == 0x00000001) {
                rb->close(fd);
                rb->splash(HZ*2, "Aborted by POWER key");
                return PLUGIN_ERROR;
            }
        }
    }

    rb->close(fd);
    {
        char msg[48];
        long mb = total_pages * g_bytes_per_page / (1024*1024);
        rb->snprintf(msg, sizeof(msg), "Done! ~%ld MB -> %s", mb, DUMP_FILE);
        rb->splash(HZ*4, msg);
    }
    return PLUGIN_OK;
}
