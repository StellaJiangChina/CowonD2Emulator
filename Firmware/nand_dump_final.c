/***************************************************************************
 * NAND dump for Cowon D2 2GB (K9GAG08U0M, 4KB page, 2-plane)
 * - page_size=4096 data + 128 spare = 4224 bytes/page
 * - 128 pages/block, 4096 blocks/bank, 2 planes = 524288 pages = 2GB
 * - output: single file /nand_raw.bin (~2.06GB)
 * - POWER button to abort
 ***************************************************************************/

#include "plugin.h"

#define NFC_CMD    (*(volatile unsigned long *)0xF0053000)
#define NFC_SADDR  (*(volatile unsigned long *)0xF005300C)
#define NFC_WDATA  (*(volatile unsigned long *)0xF0053010)
#define NFC_SDATA  (*(volatile unsigned long *)0xF0053040)
#define NFC_CTRL   (*(volatile unsigned long *)0xF0053050)
#define NFC_RST    (*(volatile unsigned long *)0xF0053064)

#define BCLKCTR    (*(volatile unsigned long *)0xF3000018)
#define DEV_NAND   (1<<9)

#define GPIOB_DIR   (*(volatile unsigned long *)0xF005A040)
#define GPIOB_SET   (*(volatile unsigned long *)0xF005A044)
#define GPIOB_CLEAR (*(volatile unsigned long *)0xF005A04C)

#define NFC_CS0    (1<<23)
#define NFC_CS1    (1<<22)
#define NFC_READY  (1<<20)
#define NFC_16BIT  (1<<26)
#define GPIO_CS    (1<<21)
#define GPIO_WE    (1<<19)

#define PAGE_RAW_SIZE   4224  /* 4096 data + 128 spare */
#define TOTAL_PAGES     524288
#define PAGES_PER_BATCH 16

static char line[128];
static int  row;

static void disp_line(const char *s)
{
    rb->lcd_puts(0, row, s);
    rb->lcd_update();
    row++;
    if (row > 12) row = 0;
}

static void nand_chip_select(int bank)
{
    if (bank & 1) { NFC_CTRL &= ~NFC_CS0; NFC_CTRL |= NFC_CS1; }
    else          { NFC_CTRL |= NFC_CS0;  NFC_CTRL &= ~NFC_CS1; }
    if (bank & 2) GPIOB_SET = GPIO_CS;
    else          GPIOB_CLEAR = GPIO_CS;
}

static void nand_reset(void)
{
    int t;
    NFC_CMD = 0xFF;
    for (t = 0; t < 200000; t++)
        if (NFC_CTRL & NFC_READY) break;
}

static void nand_init(void)
{
    BCLKCTR |= DEV_NAND;
    GPIOB_DIR |= (GPIO_CS | GPIO_WE);
    nand_chip_select(0);
    GPIOB_CLEAR = GPIO_WE;
    NFC_CTRL = (NFC_CTRL & ~0xFFF) | 0x131;
    NFC_CTRL &= ~NFC_16BIT;
    nand_reset();
}

/* Read one full 4224-byte page from col 0 */
static void read_page(unsigned long page, unsigned char *buf)
{
    int i;
    unsigned int *ptr = (unsigned int *)buf;
    NFC_CTRL = (NFC_CTRL & ~0xFFF) | 0x131;
    nand_chip_select(0);
    GPIOB_CLEAR = GPIO_WE;
    NFC_CTRL &= ~NFC_16BIT;
    NFC_CMD = 0x00;
    NFC_SADDR = 0;  /* col low */
    NFC_SADDR = 0;  /* col high */
    NFC_SADDR = page & 0xFF;
    NFC_SADDR = (page >> 8) & 0xFF;
    NFC_SADDR = (page >> 16) & 0xFF;
    NFC_CMD = 0x30;
    while (!(NFC_CTRL & NFC_READY)) {}
    for (i = 0; i < (PAGE_RAW_SIZE / 4); i++)
        *ptr++ = NFC_WDATA;
}

static int check_abort(void)
{
    return (rb->button_get_w_tmo(0) & BUTTON_POWER) ? 1 : 0;
}

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    unsigned char *buf;
    size_t buf_size;
    int fd = -1;
    unsigned long page, done = 0;
    int batch;

    rb->lcd_clear_display();
    row = 0;
    disp_line("NAND dump 4K 2GB (1 file)");

    nand_init();

    buf = rb->plugin_get_audio_buffer(&buf_size);
    if (buf_size < (PAGE_RAW_SIZE * PAGES_PER_BATCH + 4096)) {
        disp_line("buffer too small");
        goto end;
    }

    fd = rb->open("/nand_raw.bin", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        disp_line("open nand_raw.bin fail");
        goto end;
    }

    for (page = 0; page < TOTAL_PAGES; page += PAGES_PER_BATCH) {
        if (check_abort()) {
            disp_line("ABORTED by POWER");
            rb->close(fd);
            goto end;
        }

        int pages_this = PAGES_PER_BATCH;
        if (page + pages_this > TOTAL_PAGES)
            pages_this = TOTAL_PAGES - page;

        for (batch = 0; batch < pages_this; batch++) {
            read_page(page + batch, buf + batch * PAGE_RAW_SIZE);
        }

        rb->write(fd, buf, pages_this * PAGE_RAW_SIZE);

        done = page + pages_this;
        if ((done % 8192) == 0 || done == TOTAL_PAGES) {
            rb->snprintf(line, sizeof(line), "%lu/%lu (%.1f%%)",
                done, TOTAL_PAGES, (float)done * 100.0 / TOTAL_PAGES);
            disp_line(line);
        }
    }

    rb->close(fd);
    rb->storage_sleep();

    rb->snprintf(line, sizeof(line), "Done: %lu pages", done);
    disp_line(line);
    disp_line("nand_raw.bin (~2.06GB)");

end:
    disp_line("POWER to exit");
    for (;;) {
        if (rb->button_get_w_tmo(HZ / 5) & BUTTON_POWER)
            return PLUGIN_OK;
    }
}
