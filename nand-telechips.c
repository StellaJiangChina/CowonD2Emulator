/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: ata-nand-telechips.c 21933 2009-07-17 22:28:49Z gevaerts $
 *
 * Copyright (C) 2008 Rob Purchase
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "nand.h"
#include "ata-nand-target.h"
#include "system.h"
#include <string.h>
#include "led.h"
#include "panic.h"
#include "nand_id.h"
#include "storage.h"
#include "buffer.h"
#include "led.h"

#define SECTOR_SIZE 512

/** static, private data **/
static bool initialized = false;

static long next_yield = 0;
#define MIN_YIELD_PERIOD 1000

static struct mutex ata_mtx SHAREDBSS_ATTR;

#define MAX_WRITE_CACHES    8

/* Sector type identifiers - main data area */

#define SECTYPE_MAIN_LPT           0x12
#define SECTYPE_MAIN_DATA          0x13
#define SECTYPE_MAIN_RANDOM_CACHE  0x15
#define SECTYPE_MAIN_INPLACE_CACHE 0x17
/* We don't touch the hidden area at all - these are for reference */
#define SECTYPE_HIDDEN_LPT           0x22
#define SECTYPE_HIDDEN_DATA          0x23
#define SECTYPE_HIDDEN_RANDOM_CACHE  0x25
#define SECTYPE_HIDDEN_INPLACE_CACHE 0x27
#define SECTYPE_FIRMWARE             0xE0

/* Offsets to data within sector's spare area */
#define OFF_CACHE_PAGE_LOBYTE 2
#define OFF_CACHE_PAGE_HIBYTE 3
#define OFF_SECTOR_TYPE       4
#define OFF_LOG_SEG_LOBYTE    7
#define OFF_LOG_SEG_HIBYTE    6

       int total_banks         = 0;
static int sectors_per_page    = 0;
static int bytes_per_segment   = 0;
static int sectors_per_segment = 0;
static int segments_per_bank   = 0;
static int pages_per_segment   = 0;

/* Maximum values for static buffers */

#define MAX_PAGE_SIZE          4096
#define MAX_SPARE_SIZE         128
#define MAX_BLOCKS_PER_BANK    8192
#define MAX_PAGES_PER_BLOCK    128
#define MAX_BANKS              4
#define MAX_BLOCKS_PER_SEGMENT 4

#define MAX_SEGMENTS (MAX_BLOCKS_PER_BANK * MAX_BANKS / MAX_BLOCKS_PER_SEGMENT)

/* Logical/Physical translation table */
struct lpt_entry
{
    short bank;
    short phys_segment;
};
#ifdef BOOTLOADER
static struct lpt_entry lpt_lookup[MAX_SEGMENTS];
#else
/* buffer_alloc'd in nand_init() when the correct size has been determined */
static struct lpt_entry* lpt_lookup = NULL;
#endif

struct write_cache
{
    short log_segment;
    short inplace_bank;
    short inplace_phys_segment;
    short inplace_pages_used;
    short random_bank;
    short random_phys_segment;
    short page_map[MAX_PAGES_PER_BLOCK * MAX_BLOCKS_PER_SEGMENT];
};
static struct write_cache write_caches[MAX_WRITE_CACHES];
static int write_caches_in_use = 0;

struct nand_info
{
    unsigned char     dev_id;
    unsigned char     dev_id2;
    unsigned short    pages_per_block;
    unsigned short    blocks_per_bank;
    unsigned short    page_size;
    unsigned short    spare_size;
    unsigned char     col_cycles;
    unsigned char     row_cycles;
    unsigned char     planes;
};
/*                            id,  id2, pages,blocks,psize,spare,col,row,planes */
struct nand_info nd_info = {0xD5, 0x14,  128, 4096,  4096,  128,  2,  3,   2 }; /* K9GAG08UOM */
static struct nand_info* nand_data = &nd_info;
/*
     4096blocks  with 128pages  with 4096Bytes, 2 planes
        4096         128            4096
size = blocks * pages_per_block * page_size;
page   0...524287
offset 0...4095

*/
/* Conversion functions */
static int phys_segment_to_page(int phys_segment, int page_in_seg)
{
    int page = 0;

    switch (nand_data->planes)
    {
        case 1: page = phys_segment * nand_data->pages_per_block; break;

        case 2:
        case 4: page = phys_segment * nand_data->pages_per_block * 2;

                if (page_in_seg & 1) /* Data is located in block+1 */
                    page += nand_data->pages_per_block;

                if (nand_data->planes == 4 && page_in_seg & 2) /* Data is located in 2nd half of bank */
                    page += (nand_data->blocks_per_bank/2) * nand_data->pages_per_block;
    }
    
    return page + page_in_seg / nand_data->planes;
}


static void nand_read_id(int bank, unsigned char* id_buf)
{
    int i;
    
    BCLKCTR |= DEV_NAND;   /* Enable NFC bus clock */
    NFC_RST = 0;           /* Reset NAND controller */
    NFC_CTRL = (NFC_CTRL &~0xFFF) | 0x353; /* Set slow cycle timings since the chip is as yet unidentified */
    NAND_GPIO_CLEAR(WE_GPIO_BIT); /* Set write protect */
    NFC_CMD = 0xFF;               /* Reset command */
    NFC_CTRL &= ~NFC_16BIT;       /* Set 8-bit data width */

    NFC_CMD   = 0x90;       /* Read ID command, single address cycle */
    NFC_SADDR = 0x00;
    /* Read the 5 chip ID bytes */
    for (i = 0; i < 5; i++)
        id_buf[i] = NFC_SDATA & 0xFF;

    NFC_CTRL = 0x10E905B6; /* Set to original fw settings */
    BCLKCTR &= ~DEV_NAND;  /* Disable NFC bus clock */
}


static void nand_read_uid(int bank, unsigned int* uid_buf)
{
    int i;

    BCLKCTR |= DEV_NAND;                   /* Enable NFC bus clock */
    NFC_CTRL = (NFC_CTRL &~0xFFF) | 0x132; /* Set cycle timing (stp = 1, pw = 3, hold = 1) */
    NAND_GPIO_CLEAR(WE_GPIO_BIT);          /* Set write protect */
    NFC_CTRL &= ~NFC_16BIT;                /* Set 8-bit data width */

    /* Undocumented (SAMSUNG specific?) commands set the chip into a
       special mode allowing a normally-hidden UID block to be read. */
    NFC_CMD = 0x30;
    NFC_CMD = 0x65;
    NFC_CMD = 0x00; /* Read command */

    /* Write row/column address */
    for (i = 0; i < nand_data->col_cycles; i++) NFC_SADDR = 0;
    for (i = 0; i < nand_data->row_cycles; i++) NFC_SADDR = 0;

    NFC_CMD = 0x30; /* End of read */

    while (!(NFC_CTRL & NFC_READY));  /* Wait until complete */

    for (i = 0; i < 8; i++)
        uid_buf[i] = NFC_WDATA;       /* Copy data to buffer (data repeats after 8 words) */

    NFC_CMD = 0xFF;                   /* Reset the chip back to normal mode */
    BCLKCTR &= ~DEV_NAND;             /* Disable NFC bus clock */
}


static void nand_read_raw(int bank, int page, int offset, int size, void* buf)
{
    int i;

    BCLKCTR |= DEV_NAND;                   /* Enable NFC bus clock */
    NFC_CTRL = (NFC_CTRL &~0xFFF) | 0x132; /* Set cycle timing (stp = 1, pw = 3, hold = 1) */
    NAND_GPIO_CLEAR(WE_GPIO_BIT);          /* Set write protect */
    NFC_CTRL &= ~NFC_16BIT;                /* Set 8-bit data width */
    NFC_CMD  = 0x00;                       /* Read command */

    for (i = 0; i < nand_data->col_cycles; i++) /* Write column address */
    { NFC_SADDR = offset & 0xFF; offset = offset >> 8; }

    for (i = 0; i < nand_data->row_cycles; i++) /* Write page address */
    { NFC_SADDR = page & 0xFF;    page = page >> 8; }

    NFC_CMD = 0x30;    /* End of read command */
    while (!(NFC_CTRL & NFC_READY));  /* Wait until complete */

    /* Read data into page buffer */
    for (i = 0; i < (size/4); i++)
       ((unsigned int*)buf)[i] = NFC_WDATA;

    BCLKCTR &= ~DEV_NAND;    /* Disable NFC bus clock */
}


static bool nand_read_sector_of_logical_segment(int log_segment, int sector, void* buf)
{
    int page;
    int page_in_segment = sector / sectors_per_page;
    int sector_in_page  = sector % sectors_per_page;
    int bank            = lpt_lookup[log_segment].bank;
    int phys_segment    = lpt_lookup[log_segment].phys_segment;

    /* Check if any of the write caches refer to this segment/page.
       If present we need to read the cached page instead. */
    int cache_num = 0;
    bool found = false;
    
    while (!found && cache_num < write_caches_in_use)
    {
        if (write_caches[cache_num].log_segment == log_segment)
        {
            if (write_caches[cache_num].page_map[page_in_segment] != -1)
            {
                /* data is located in random pages cache */
                found = true;
                
                bank = write_caches[cache_num].random_bank;
                phys_segment = write_caches[cache_num].random_phys_segment;
                page_in_segment = write_caches[cache_num].page_map[page_in_segment];
            }
            else if (write_caches[cache_num].inplace_pages_used != -1 &&
                     write_caches[cache_num].inplace_pages_used > page_in_segment)
            {
                /* data is located in in-place pages cache */
                found = true;
                
                bank = write_caches[cache_num].inplace_bank;
                phys_segment = write_caches[cache_num].inplace_phys_segment;
            }
        }
        cache_num++;
    }

    page = phys_segment_to_page(phys_segment, page_in_segment);

    nand_read_raw(bank, page, sector_in_page * (SECTOR_SIZE+16), SECTOR_SIZE, buf);

    return true;
}


/* Miscellaneous helper functions */
static inline unsigned char get_sector_type(char* spare_buf)
{
    return spare_buf[OFF_SECTOR_TYPE];
}

static inline unsigned short get_log_segment_id(int phys_seg, char* spare_buf)
{
    return ((spare_buf[OFF_LOG_SEG_HIBYTE] << 8) | spare_buf[OFF_LOG_SEG_LOBYTE]);
}

static inline unsigned short get_cached_page_id(char* spare_buf)
{
    return (spare_buf[OFF_CACHE_PAGE_HIBYTE] << 8) | spare_buf[OFF_CACHE_PAGE_LOBYTE];
}

static int find_write_cache(int log_segment)
{
    int i;

    for (i = 0; i < write_caches_in_use; i++)
        if (write_caches[i].log_segment == log_segment)
            return i;

    return -1;
}


static void read_random_writes_cache(int bank, int phys_segment)
{
    int page = 0;
    short log_segment;
    unsigned char spare_buf[16];

    nand_read_raw(bank, phys_segment_to_page(phys_segment, page), SECTOR_SIZE, 16, spare_buf);

    log_segment = get_log_segment_id(phys_segment, spare_buf);
    
    if (log_segment == -1)
        return;

    /* Find which cache this is related to */
    int cache_no = find_write_cache(log_segment);

    if (cache_no == -1)
    {
        if (write_caches_in_use < MAX_WRITE_CACHES)
        {
            cache_no = write_caches_in_use;
            write_caches_in_use++;
        }
        else
        {
            panicf("Max NAND write caches reached");
        }
    }

    write_caches[cache_no].log_segment = log_segment;
    write_caches[cache_no].random_bank = bank;
    write_caches[cache_no].random_phys_segment = phys_segment;

    /* Loop over each page in the phys segment (from page 1 onwards).
       Read spare for 1st sector, store location of page in array. */
    for (page = 1; page < (nand_data->pages_per_block * nand_data->planes); page++)
    {
        unsigned short cached_page;
        
        nand_read_raw(bank, phys_segment_to_page(phys_segment, page), SECTOR_SIZE, 16, spare_buf);

        cached_page = get_cached_page_id(spare_buf);
        
        if (cached_page != 0xFFFF)
            write_caches[cache_no].page_map[cached_page] = page;
    }
}


static void read_inplace_writes_cache(int bank, int phys_segment)
{
    int page = 0;
    short log_segment;
    unsigned char spare_buf[16];

    nand_read_raw(bank, phys_segment_to_page(phys_segment, page), SECTOR_SIZE, 16, spare_buf);

    log_segment = get_log_segment_id(phys_segment, spare_buf);
    
    if (log_segment == -1)
        return;
    
    /* Find which cache this is related to */
    int cache_no = find_write_cache(log_segment);

    if (cache_no == -1)
    {
        if (write_caches_in_use < MAX_WRITE_CACHES)
        {
            cache_no = write_caches_in_use;
            write_caches_in_use++;
        }
        else
            panicf("Max NAND write caches reached");
    }

    write_caches[cache_no].log_segment = log_segment;
    
    /* Find how many pages have been written to the new segment */
    while (log_segment != -1 &&
           page < (nand_data->pages_per_block * nand_data->planes) - 1)
    {
        page++;
        nand_read_raw(bank, phys_segment_to_page(phys_segment, page), SECTOR_SIZE, 16, spare_buf);

        log_segment = get_log_segment_id(phys_segment, spare_buf);
    }
    
    if (page != 0)
    {
        write_caches[cache_no].inplace_bank = bank;
        write_caches[cache_no].inplace_phys_segment = phys_segment;
        write_caches[cache_no].inplace_pages_used = page;
    }
}


int nand_read_sectors(IF_MD2(int drive,) unsigned long start, int incount, void* inbuf)
{
    int ret = 0;
    
    mutex_lock(&ata_mtx);
    
    while (incount > 0)
    {
        int done = 0;
        int segment = start / sectors_per_segment;
        int secmod  = start % sectors_per_segment;

        while (incount > 0 && secmod < sectors_per_segment)
        {
            if (!nand_read_sector_of_logical_segment(segment, secmod, inbuf))
            {
                ret = -1;
                goto nand_read_error;
            }

            if (TIME_AFTER(USEC_TIMER, next_yield))
            {
                next_yield = USEC_TIMER + MIN_YIELD_PERIOD;
                yield();
            }

            inbuf += SECTOR_SIZE;
            incount--;
            secmod++;
            done++;
        }
    
        if (done < 0)
        {
            ret = -1;
            goto nand_read_error;
        }
        start += done;
    }

nand_read_error:
    mutex_unlock(&ata_mtx);
    
    return ret;
}

int nand_init(void)
{
    int bank, phys_segment, lptbuf_size;
    unsigned char spare_buf[16];

    if (initialized) return 0;
    
    mutex_init(&ata_mtx);

    /* Set GPIO direction for chip select & write protect */
    NAND_GPIO_OUT_EN(CS_GPIO_BIT | WE_GPIO_BIT);

    /* Get chip characteristics and number of banks */
    segments_per_bank   =    2048; //->blocks_per_bank / ->planes;  (4096/2)
    bytes_per_segment   = 1048576; //->page_size * ->pages_per_block * ->planes; (4096*128*2)
    sectors_per_page    =       8; //->page_size / SECTOR_SIZE;
    sectors_per_segment =    2048; // bytes_per_segment / SECTOR_SIZE;
    pages_per_segment   =     256; // sectors_per_segment  / sectors_per_page;
    total_banks         =       1;

    /* Use chip info to allocate the correct size LPT buffer */
    lptbuf_size = sizeof(struct lpt_entry) * segments_per_bank * total_banks;
    lpt_lookup = buffer_alloc(lptbuf_size);

    memset(lpt_lookup, 0xff, lptbuf_size);
    memset(write_caches, 0xff, sizeof(write_caches));
    
    write_caches_in_use = 0;

    /* Scan banks to build up block translation table */
    for (bank = 0; bank < total_banks; bank++)
    {
        for (phys_segment = 0; phys_segment < segments_per_bank; phys_segment++)
        {
            /* Read spare bytes from first sector of each segment */
            nand_read_raw(bank, phys_segment_to_page(phys_segment, 0), SECTOR_SIZE, 16, spare_buf);
            
            int type = get_sector_type(spare_buf);

            if (type == SECTYPE_MAIN_INPLACE_CACHE)
            {
                /* Since this type of segment is written to sequentially, its
                   job is complete if the final page has been written. In this
                   case we need to treat it as a normal data segment. */
                nand_read_raw(bank, phys_segment_to_page(phys_segment, pages_per_segment - 1), SECTOR_SIZE, 16, spare_buf);
                
                if (get_sector_type(spare_buf) != 0xff)
                {
                    type = SECTYPE_MAIN_DATA;
                }
            }

            switch (type)
            {
                case SECTYPE_MAIN_DATA:
                {
                    /* Main data area segment */
                    unsigned short log_segment = get_log_segment_id(phys_segment, spare_buf);

                    if (log_segment < segments_per_bank * total_banks)
                    {
                        if (lpt_lookup[log_segment].bank == -1 ||
                            lpt_lookup[log_segment].phys_segment == -1)
                        {
                            lpt_lookup[log_segment].bank = bank;
                            lpt_lookup[log_segment].phys_segment = phys_segment;
                        }
                        else
                        {
                            //panicf("duplicate data segment 0x%x!", log_segment);
                        }
                    }
                    break;
                }
                
                case SECTYPE_MAIN_RANDOM_CACHE:
                {
                    /* Newly-written random page data (Main data area) */
                    read_random_writes_cache(bank, phys_segment);
                    break;
                }
                
                case SECTYPE_MAIN_INPLACE_CACHE:
                {
                    /* Newly-written sequential page data (Main data area) */
                    read_inplace_writes_cache(bank, phys_segment);
                    break;
                }
            }
        }
    }
    
    initialized = true;

    return 0;
}
