#include "cowond2.h"
#include "tcc780x.h"

#define USB_DETECT

extern int xMouse, yMouse;

LONG  search_register(ULONG des);
ULONG *get_register(ULONG des);

unsigned char uid[256] = {
0xEC,0x00,0x00,0xB4,0x37,0xF8,0x34,0x30,0x34,0x32,0x30,0x32,0x05,0x05,0x09,0x08,
0x13,0xFF,0xFF,0x4B,0xC8,0x07,0xCB,0xCF,0xCB,0xCD,0xCF,0xCD,0xFA,0xFA,0xF6,0xF7,
0xEC,0x00,0x00,0xB4,0x37,0xF8,0x34,0x30,0x34,0x32,0x30,0x32,0x05,0x05,0x09,0x08,
0x13,0xFF,0xFF,0x4B,0xC8,0x07,0xCB,0xCF,0xCB,0xCD,0xCF,0xCD,0xFA,0xFA,0xF6,0xF7,
0xEC,0x00,0x00,0xB4,0x37,0xF8,0x34,0x30,0x34,0x32,0x30,0x32,0x05,0x05,0x09,0x08,
0x13,0xFF,0xFF,0x4B,0xC8,0x07,0xCB,0xCF,0xCB,0xCD,0xCF,0xCD,0xFA,0xFA,0xF6,0xF7,
0xEC,0x00,0x00,0xB4,0x37,0xF8,0x34,0x30,0x34,0x32,0x30,0x32,0x05,0x05,0x09,0x08,
0x13,0xFF,0xFF,0x4B,0xC8,0x07,0xCB,0xCF,0xCB,0xCD,0xCF,0xCD,0xFA,0xFA,0xF6,0xF7,
0xEC,0x00,0x00,0xB4,0x37,0xF8,0x34,0x30,0x34,0x32,0x30,0x32,0x05,0x05,0x09,0x08,
0x13,0xFF,0xFF,0x4B,0xC8,0x07,0xCB,0xCF,0xCB,0xCD,0xCF,0xCD,0xFA,0xFA,0xF6,0xF7,
0xEC,0x00,0x00,0xB4,0x37,0xF8,0x34,0x30,0x34,0x32,0x30,0x32,0x05,0x05,0x09,0x08,
0x13,0xFF,0xFF,0x4B,0xC8,0x07,0xCB,0xCF,0xCB,0xCD,0xCF,0xCD,0xFA,0xFA,0xF6,0xF7,
0xEC,0x00,0x00,0xB4,0x37,0xF8,0x34,0x30,0x34,0x32,0x30,0x32,0x05,0x05,0x09,0x08,
0x13,0xFF,0xFF,0x4B,0xC8,0x07,0xCB,0xCF,0xCB,0xCD,0xCF,0xCD,0xFA,0xFA,0xF6,0xF7,
0xEC,0x00,0x00,0xB4,0x37,0xF8,0x34,0x30,0x34,0x32,0x30,0x32,0x05,0x05,0x09,0x08,
0x13,0xFF,0xFF,0x4B,0xC8,0x07,0xCB,0xCF,0xCB,0xCD,0xCF,0xCD,0xFA,0xFA,0xF6,0xF7 };

char   *rdwrstg[20] = { "W1","W1","W1","W1","W2","W2","W4","R1","R1","R1","R1","R1","R1","R1","R1","R2","R2","R2","R2","R4" };
FILE   *fil, *atafile, *offfile, *regfile, *accfile;
int    pc;
ULONG  iii;
ULONG  bootfile_active;
ULONG  bootfile_offset;
ULONG  ctrue;
ULONG  iram[IRAM_SIZE/4];
ULONG  dram[DRAM_SIZE/4];
ULONG  cach[CACH_SIZE/4];
ULONG  eepr[EEPR_SIZE/4];
#define NUM_SECTOR 0x4000
ULONG  wrbin[0x40000 + NUM_SECTOR*2 + NUM_SECTOR*0x210/4]; /* bitfield + directory + sector data */
ULONG  wrnum;

CORE   cpu;
ULONG  nptr;
ULONG  iptr[512]; // target register addresses
ULONG  optr[512]; // mapped register addresses
ULONG  mptr[512]; // ordered register addresses (for quick search)
ULONG  aptr[512]; // #accessed register addresses
ULONG  bptr[512]; // #write accesses to register addresses
ULONG  idat[512][256]; // written data to iptr

ULONG  tim1, timer1_limit = 0x40000; // #instr per interrupt
ULONG  tim2, timer2_limit = 0x03333; // #instr per interrupt

/* globals used in check_for_loop() */
ASM_BUFFER asmbuf[2];

#define UINT64 unsigned __int64

ULONG  *cpu_mbox0;
ULONG  *nfc_cmd, cmd;
ULONG  *nfc_saddr;
ULONG  *nfc_wdata;
ULONG  *nfc_sdata;
ULONG  *nfc_sdata;
ULONG  *nfc_ctrl;
ULONG  *nfc_ireq;
ULONG  *nfc_rst;
UCHAR  nfc_addr_arr[8], nfc_dind, nfc_aind, nfc_anum;
UINT64 nfc_addr, nfc_sadd;

unsigned int  nfc_id[5] = { 0xEC,0xD5,0x14,0xB6,0x74 };
unsigned int  resp70[2] = { 0x40,0x40 };
unsigned int  nfc_indx_snap;
unsigned int  nfc_cmnd_snap;
unsigned int  nfc_ctrl_snap;
unsigned int  nfc_dcnt_snap;
unsigned char nfc_dcnt_data[20];
unsigned int  nfc_type_snap; /* data type */

int partition = -1;
unsigned char *rptr;

void *read_cache_file(UINT64 offset)
{
  ULONG i, sector = (ULONG)(offset / 0x210); /* 23 + 9 */

  if(sector >= 0x800000)
    SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"illegal sector access");

  if(wrbin[sector>>5] & (1 << (sector & 31)))
    for(i=0; i<NUM_SECTOR; i++)
      if(wrbin[0x40000+2*i] == sector)
        return (UCHAR*)wrbin + wrbin[0x40000+2*i+1] + offset % 0x210;

  return NULL;
}

UINT64 read_value(UINT64 offset, ULONG *ptr, ULONG sz)
{
  void *p;
  ULONG lval = 0xffffffff;

  if((p = read_cache_file(offset)) == NULL)
  {
    if(offset < 0xEBDBD100 && partition >= 0)
    {
      _lseeki64(partition, offset, SEEK_SET);
      _read(partition, &lval, sz);
    }
    else
      lval = 0xffffffff;
  }
  else
  {
         if(sz == 1) lval = *(UCHAR *)p;
    else if(sz == 2) lval = *(USHORT*)p;
    else             lval = *(ULONG *)p;
  }

       if(sz == 1)  *ptr = lval & 0xff;
  else if(sz == 2)  *ptr = (((USHORT)lval & 0xff00) << 8) | ((USHORT)lval & 0x00ff);
  else              *ptr = lval;

  return offset + sz;
}

UINT64 write_value(UINT64 offset, ULONG lval, ULONG sz)
{
  ULONG i, sector = (ULONG)(offset / 0x210); /* 23 + 9 */

  if(sector >= 0x800000)
    SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"illegal sector access");

  if(sz != 4)
    SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"illegal write amount");

  if(wrbin[sector>>5] & (1 << (sector & 31))) /* overwrite */
  {
    for(i=0; i<NUM_SECTOR; i++)
      if(wrbin[0x40000+2*i] == sector)
      { wrbin[(wrbin[0x40000+2*i+1] + offset%0x210) >> 2] = lval; break; }
  }
  else
  {
    if(wrnum + 1 == NUM_SECTOR)
      SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"too many write sectors");
    else
    {
      wrbin[sector>>5] |= 1 << (sector & 31); /* new write sector */
      wrbin[0x40000+2*wrnum+0] = sector;
      wrbin[0x40000+2*wrnum+1] = 0x100000 + NUM_SECTOR * 8 + wrnum * 0x210;
      wrbin[(wrbin[0x40000+2*wrnum+1] + offset%0x210) >> 2] = lval;
      wrnum++;
    }
  }

  return offset + sz;
}

void dump_snap_and_reinit(ULONG val)
{
  int        i;
  ULONG      lval;
  const char *type[] = { "--", "R1", "R4", "W1", "W4" };

  if(offfile && nfc_indx_snap)
  {
    fprintf(offfile, "%6x %4x %08x ", nfc_indx_snap, nfc_cmnd_snap, nfc_ctrl_snap);
    lval = (ULONG)(nfc_sadd / 0x1080);
    for(i=0; i<3; i++)  fprintf(offfile, "%02x", (lval >> (16-8*i)) & 0xff);
    fprintf(offfile, "-");
    lval = (ULONG)(nfc_sadd % 0x1080);
    for(i=0; i<2; i++)  fprintf(offfile, "%02x", (lval >>  (8-8*i)) & 0xff);
    fprintf(offfile, " %05x %s ", nfc_dcnt_snap, type[nfc_type_snap]);
    for(i=0; i<nfc_dcnt_snap && i<20; i+=4)
      fprintf(offfile, " %08x", *(ULONG*)&nfc_dcnt_data[i]);
    fprintf(offfile, "\n");
  }
  nfc_indx_snap = iii;
  nfc_cmnd_snap = val;
  nfc_ctrl_snap = 0;
  nfc_dcnt_snap = 0;
  nfc_type_snap = 0;
  nfc_sadd      = nfc_addr;
}

void emu_nand_nfc(ULONG des, ULONG *ptr, ULONG typ, ULONG val)
{
  ULONG *dst, *src, l, len, lval;

  if(partition == -1)
  {
    FILE *fil = NULL;//fopen("wrdata.bin", "rb");
    /* format of this flash binary: n x (8 x (512+32)) of physical sector data */
    partition = _open("D:\\nand_perfect.bin", _O_RDONLY);

    if(fil != NULL)
    { fread(&wrnum, sizeof(wrnum), 1, fil);
      fread(&wrbin, sizeof(wrbin), 1, fil);
      fclose(fil);
    }
  }

  switch(des)
  {
/* NFC_CMD   */ case 0xF0053000: nfc_cmd = ptr;
                                 dump_snap_and_reinit(val);
                                 nfc_ctrl_snap = nfc_ctrl ? *nfc_ctrl : 0;

                                 if(typ < 7) /* reset counters on write access */
                                 {
                                   nfc_dind = nfc_aind = 0;

                                   if(val == 0) /* read command */
                                     nfc_addr = 0;

                                   if(val == 0xff || val == 0xffff) /* reset command */
                                     rptr = NULL;

                                   if(val == 0x65 || val == 0x6565) /* uid mode */
                                     rptr = uid;
                                 }
                                 break;

/* NFC_SADDR */ case 0xF005300C: nfc_saddr = ptr;
                                 nfc_addr_arr[nfc_aind++] = (UCHAR)val;

                                 switch(*nfc_cmd & 0xff)
                                 {
                                 case 0xf1: nfc_anum = 0; break; /* read id    */
                                 case 0xf2: nfc_anum = 0; break; /* read id    */
                                 case 0xff: nfc_anum = 0; break; /* read id    */
                                 case 0x90: nfc_anum = 1; break; /* read id    */
                                 case 0x60: nfc_anum = 3; break; /* clear page */
                                 default:   nfc_anum = 5; break; /* others     */
                                 }

                                 if(nfc_aind == nfc_anum)
                                 {
                                   nfc_addr_arr[nfc_anum] = 0;
                                   switch(*nfc_cmd & 0xff)
                                   {
                                   case 0xf1: case 0xf2: case 0xff:       break;
                                   case 0x90: nfc_sadd = nfc_addr_arr[0]; break;
                                   case 0x60: nfc_sadd = *(ULONG*)&nfc_addr_arr[0] * 0x1080; break;
                                   default:   nfc_sadd = *(USHORT*)&nfc_addr_arr[0] + (UINT64)*(ULONG*)&nfc_addr_arr[2] * 0x1080; break;
                                   }
                                   nfc_addr = nfc_sadd;
                                 }
                                 break;

/* NFC_WDATA */ case 0xF0053010: if(nfc_ctrl)
                                 {
                                   nfc_wdata      = ptr;
                                   nfc_type_snap  = 2 + (typ < 7 ? 2 : 0);
                                   nfc_ctrl_snap  = *nfc_ctrl;

                                   if((*nfc_ctrl & 0xc00000) != 0x800000) *ptr = 0;
                                   else
                                   if(rptr) { *ptr = *(ULONG*)&rptr[nfc_addr]; nfc_addr += 4; }
                                   else
                                   switch(*nfc_cmd)
                                   {
                                     case 0x0030: /* read raw */
                                     nfc_addr = read_value(nfc_addr, ptr, 4); val = *ptr; break;

                                     case 0x3030: /* read raw */
                                     nfc_addr = read_value(nfc_addr, ptr, 2); val = *ptr; break;

                                     case 0x0080: /* write raw */
                                     case 0x0081: /* write raw */
                                     nfc_addr = write_value(nfc_addr, val, 4); break;
                                   }
                                   if(nfc_dcnt_snap < 20)
                                     *(ULONG*)&nfc_dcnt_data[nfc_dcnt_snap] = val;
                                   nfc_dcnt_snap += 4;
                                 }
                                 break;

/* NFC_SDATA */ case 0xF0053040: if(nfc_ctrl)
                                 { 
                                   nfc_sdata = ptr;
                                   nfc_type_snap = 1 + (typ < 7 ? 2 : 0);
                                   nfc_ctrl_snap = *nfc_ctrl;
                                   if((*nfc_ctrl & 0xc00000) != 0x800000) *ptr = 0; else
                                   switch(*nfc_cmd)
                                   {
/* read raw */                     case 0x0030:
                                   case 0x3030: *ptr = rptr[nfc_addr++];   break;
/* read  id */                     case 0x0090:
/* read  id */                     case 0x9090: *ptr = nfc_id[nfc_dind++]; break;
                                   case 0x0070:
                                   case 0x7070: *ptr = resp70[nfc_dind++]; break;
                                   case 0x00f1:
                                   case 0xf1f1: *ptr = 0x00C0;             break;
                                   }
                                   if(nfc_dcnt_snap < 20)
                                     nfc_dcnt_data[nfc_dcnt_snap] = (UCHAR)*ptr;
                                   nfc_dcnt_snap++;
                                 }
                                 break;

/* NFC_CTRL  */ case 0xF0053050: nfc_ctrl  = ptr; *ptr = *ptr | NFC_READY; break;
/* NFC_IREQ  */ case 0xF0053060: nfc_ireq  = ptr; break;
/* NFC_RST   */ case 0xF0053064: nfc_rst   = ptr; break;

/*************************************** DMA *********************************************/
/* NFC_DMA_RD*/ case 0xF0053058: dst =  get_register((ULONG)DMA_DES);
                                 len = *get_register((ULONG)&NFC_DMA_SZ);
                                 nfc_type_snap = 2;
                                 nfc_ctrl_snap = *nfc_ctrl;
                                 for(l=0; l<len; l+=4)
                                 { 
                                   if((*nfc_ctrl & 0xc00000) == 0x800000)
                                   {
                                     nfc_addr = read_value(nfc_addr, &lval, 4);
                                   }
                                   else  lval = 0xffffffff;

                                   SETUL(*dst, lval);
                                   if(nfc_dcnt_snap < 20)
                                     *(ULONG*)&nfc_dcnt_data[nfc_dcnt_snap] = lval;
                                   nfc_dcnt_snap += 4;
                                   (*dst)   += 4;
                                 }
                                 dump_snap_and_reinit(nfc_cmnd_snap);
                                 break;

/* NFC_DMA_WR*/ case 0xF0053054: src =  get_register((ULONG)DMA_SRC);
                                 len = *get_register((ULONG)&NFC_DMA_SZ);
                                 nfc_type_snap = 4;
                                 nfc_ctrl_snap = *nfc_ctrl;
                                 for(l=0; l<len; l+=4)
                                 { 
                                   if((*nfc_ctrl & 0xc00000) == 0x800000)
                                     nfc_addr = write_value(nfc_addr, lval=GETUL(*src), 4);
                                   if(nfc_dcnt_snap < 20)
                                     *(ULONG*)&nfc_dcnt_data[nfc_dcnt_snap] = lval;
                                   nfc_dcnt_snap += 4;
                                   (*src) += 4;
                                 }
                                 dump_snap_and_reinit(nfc_cmnd_snap);
                                 break;
  }

  if(atafile)
  switch(des)
  {
/* NFC_CMD   */ case 0xF0053000: fprintf(atafile, "%6x NFC_CMD   %c %08x\n", iii, typ<7?'W':'R', typ<7?val:*ptr); break;
/* NFC_SADDR */ case 0xF005300C: fprintf(atafile, "%6x NFC_SADDR %c %08x\n", iii, typ<7?'W':'R', typ<7?val:*ptr); break;
/* NFC_WDATA */ case 0xF0053010: fprintf(atafile, "%6x NFC_WDATA %c %08x\n", iii, typ<7?'W':'R', typ<7?val:*ptr); break;
/* NFC_SDATA */ case 0xF0053040: fprintf(atafile, "%6x NFC_SDATA %c %08x\n", iii, typ<7?'W':'R', typ<7?val:*ptr); break;
/* NFC_CTRL  */ case 0xF0053050: fprintf(atafile, "%6x NFC_CTRL  %c %08x\n", iii, typ<7?'W':'R', typ<7?val:*ptr); break;
/* NFC_IREQ  */ case 0xF0053060: fprintf(atafile, "%6x NFC_IREQ  %c %08x\n", iii, typ<7?'W':'R', typ<7?val:*ptr); break;
/* NFC_RST   */ case 0xF0053064: fprintf(atafile, "%6x NFC_RST   %c %08x\n", iii, typ<7?'W':'R', typ<7?val:*ptr); break;
/* ???????   */ default:         fprintf(atafile, "%6x NFC_????? %c %08x\n", iii, typ<7?'W':'R', typ<7?val:*ptr); break;
  }
}

USHORT *lcd_buffer;
ULONG  *lcd_fb_base_reg;
ULONG  *usec_timer;
ULONG  *m_irq, *va_irq, *vn_irq, *tim_req;

ULONG emu_cpu_cop(ULONG des, ULONG *ptr, ULONG typ, ULONG val)
{
  switch(des)
  {
    case 0xF3003094:      usec_timer   = ptr;  return 1; /* USEC_TIMER: modified externally */
    case 0xF3004084:      *ptr  = 0x02010280;  return 1; /* ADCSTATUS */
    case 0xF005B070:      *ptr |= 4;           return 1; /* ECC_ERR_NUM (4=no error) */
    case 0xF005A020:      *ptr |= 0xF7FF0DCC;  return 1; // GPIOA (29=remote?,26=!sdcard)
    case 0xF005A040:      *ptr  = xMouse >= 0 ? 0x1DC60002 : 0x3DC60002;  return 1; // GPIOB
    case 0xF005A060:      *ptr |= 0x25000000;  return 1; // GPIOC
    case 0xF005A080:      *ptr |= 0x070F0000;  return 1; // GPIOD
    case 0xF005A0A0:      *ptr |= 0x0FB7B7FF;  return 1; // GPIOE
    case 0xF3003060:      tim_req      = ptr;  return 1; /* TIREQ */
    case 0xF3001028:      m_irq        = ptr;  return 1; /* MIRQ  */
    case 0xF3001080:      va_irq       = ptr;  return 1; /* VAIRQ */
    case 0xF3001088:      vn_irq       = ptr;  return 1; /* VNIRQ */
    case 0xF3001000:      cpu.int_en   = ptr;  return 1; /* IEN   */
    case 0xF0000098:      lcd_fb_base_reg = ptr;
                          if(typ < 7)
                          { *lcd_fb_base_reg = val;
                            lcd_buffer = (USHORT*)&dram[(*lcd_fb_base_reg & 0x1ffffff)>>2];
                          }
                          return 1;
  }
  return 0;
}

LONG search_register(ULONG des)
{
  LONG i=0, min, max;

  for(min=0, max=nptr-1, i=(min+max)>>1; min<max; i=(min+max)>>1)
  {
    if(iptr[mptr[i]] < des)  min = i + 1;
    else                     max = i;
  }

  return i;
}

ULONG *get_register(ULONG des)
{
  LONG i;

  if(nptr*sizeof(iptr[0]) >= sizeof(iptr))
    SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: register overflow");
  else
  {
    i = search_register(des);

    if(iptr[mptr[i]] == des) // found
    {
      i = mptr[i];
      aptr[i]++; // increment access counter
    }
    else
    {
      /* not found: insert new entry */
      memmove(mptr+i+1, mptr+i, sizeof(mptr[0])*(nptr-i));
      i = mptr[i] = nptr++;
      iptr[i] = des;

      if(i >= 512)
        SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: register overflow");

      if(iii)      /* ignore access during ptr initialization */
        aptr[i]++; // increment access counter

      /* following registers have initial preset values */
      switch(des)
      {
      case 0xf300600c: optr[i] = 8;           break; /* needed for lcd init */
      case 0xf3003060: optr[i] = TIREQ_TF0;   break;
      case 0xF0053050: optr[i] = NFC_READY;   break;
      }
    }
  }

  return &optr[i];
}

ULONG loadstore(ULONG *ptr, ULONG typ, ULONG val)
{
  switch(typ)
  {
    case WRITE10: return *ptr = (*ptr & 0xffffff00) | ((val & 0xff) <<  0);
    case WRITE11: return *ptr = (*ptr & 0xffff00ff) | ((val & 0xff) <<  8);
    case WRITE12: return *ptr = (*ptr & 0xff00ffff) | ((val & 0xff) << 16);
    case WRITE13: return *ptr = (*ptr & 0x00ffffff) | ((val & 0xff) << 24);
    case WRITE20: return *ptr = (val & 0xffff) | (*ptr & 0xffff0000);
    case WRITE22: return *ptr = (val << 16)    | (*ptr & 0x0000ffff);
    case WRITE40: return *ptr = val;
    case READU10: return       (*ptr << 24) >> 24;
    case READU11: return       (*ptr << 16) >> 24;
    case READU12: return       (*ptr <<  8) >> 24;
    case READU13: return       (*ptr <<  0) >> 24;
    case READS10: return (long)(*ptr << 24) >> 24;
    case READS11: return (long)(*ptr << 16) >> 24;
    case READS12: return (long)(*ptr <<  8) >> 24;
    case READS13: return (long)(*ptr <<  0) >> 24;
    case READU20: return       *ptr & 0xffff;
    case READU22: return       *ptr >> 16;
    case READS20: return (long)(*ptr << 16) >> 16;
    case READS22: return (long)*ptr >> 16;
    default:      return *ptr;
  }
}

unsigned int   i2c_gdat; /* GPIOA      */
unsigned int   i2c_gdir; /* GPIOA_DIR: */
unsigned int   i2c_stat; /*  */
unsigned char  i2c_addr[20];
unsigned char  i2c_dat[20];
unsigned char *i2c_data;
unsigned char  i2c_posi[2] = { 0x20, 0x80 }; /* ADCS1 + ADCS2 */
unsigned char  i2c_ints[3] = { 0x00, 0x00, 0x01 }; /* ADCRDY + TSCPRES */
unsigned char  i2c_time[7] = { 0x10, 0x10, 0x08, 0x01, 0x01, 0x09, 0x10 };
unsigned char  i2c_xpos[2] = { 0xc0, 0x81 };
unsigned char  i2c_ypos[2] = { 0xc0, 0x83 };
unsigned char  i2c_batt[2] = { 0xAA, 0x80 };
unsigned char  i2c_adcs2;
unsigned char  i2c_chan; /* 0x01:batt,0x11:xpos,0x13:ypos */
unsigned int   i2c_bytn;
unsigned int   i2c_clck;
unsigned int   i2c_pen = 0x01;

void i2cout(int type, int num)
{
  int i;

  if(xMouse >= 0)
  { 
    i2c_pen     |= 0x80; /* pen down */
    i2c_ints[2] |= 0x08;
    i2c_xpos[0] = 3*xMouse >> 2;
    i2c_xpos[1] = 0x80 | (3*xMouse & 3);
    i2c_ypos[0] = 4*yMouse >> 2;
    i2c_ypos[1] = 0x80 | (4*yMouse & 3);
    i2c_adcs2   = 0x80 | (i2c_xpos[1] & 3) | ((i2c_ypos[1] & 3) << 2);
  }
  else
  { i2c_pen     &=~0x80; /* pen up   */
    i2c_ints[2] &=~0x08;
  }

  switch(type)
  {
  case 0: fprintf(regfile, "%07x: i2c_start()\n", iii);         break;

  case 1: fprintf(regfile, "%07x: i2c_outb(", iii);
          for(i=0; i<num; i++) fprintf(regfile, "0x%02x,", i2c_dat[i]);
          fprintf(regfile, ")\n");
          if(~i2c_dat[0] & 1) memcpy(i2c_addr,i2c_dat,20);
          if(num > 2 && i2c_addr[0] == 0x10 && i2c_addr[1] == 0x2f)
            i2c_chan = i2c_addr[2];                             break;

  case 2: fprintf(regfile, "%07x: i2c_stop()\n", iii);          break;

  case 3: fprintf(regfile, "%07x: i2c_inb(", iii);
          for(i=0; i<num; i++) fprintf(regfile, "0x%02x,", i2c_dat[i]);
          fprintf(regfile, ")\n");
          if(~i2c_dat[0] & 1) memcpy(i2c_addr,i2c_dat,20);      break;

  }
}

ULONG emu_i2c(ULONG des4, ULONG *ptr, ULONG typ, ULONG val)
{
  unsigned int old_dat = i2c_gdat;

  if(typ < 7) /* write */
  {
    switch(des4)
    {
    case 0xF005A020: i2c_gdat  = val; break;              /* GPIOA       */
    case 0xF005A024: i2c_gdir  = val; break;              /* GPIOA_DIR   */
    case 0xF005A028: i2c_gdat |= (i2c_gdir & val); break; /* GPIOA_SET   */
    case 0xF005A02C: i2c_gdat &=~(i2c_gdir & val); break; /* GPIOA_CLEAR */
    }

    if((((old_dat << 2) & 12) + (i2c_gdat & 3)) == 13) /* SDA_LO while SCL_HI: start */
    {
      i2cout(0, 0);
      i2c_data = i2c_dat;
      i2c_bytn = 0;
      i2c_stat = 0;
      i2c_clck = 0;
      i2c_dat[0]=i2c_dat[1]=i2c_dat[2]=i2c_dat[3]=i2c_dat[4]=i2c_dat[5]=i2c_dat[6]=i2c_dat[7]=0;
    }
    else if((((old_dat << 2) & 12) + (i2c_gdat & 3)) ==  7) /* SDA_HI while SCL_HI: stop  */
    {
      i2cout(2, 0);
      i2c_data = i2c_dat;
      i2c_bytn = 0;
      i2c_stat = 0;
      i2c_clck = 0;
      i2c_dat[0]=i2c_dat[1]=i2c_dat[2]=i2c_dat[3]=i2c_dat[4]=i2c_dat[5]=i2c_dat[6]=i2c_dat[7]=0;
    }
    else if((((old_dat << 1) & 2) + (i2c_gdat & 1)) == 2)  /* SCL_HI => SCL_LO */
    {
      i2c_stat = 1; /* input */

      if((i2c_clck % 9) == 8 && !(i2c_gdir & 2))
      {
        i2cout(3, ++i2c_bytn);
        i2c_data++;
      }
    }
    else if((((old_dat << 1) & 2) + (i2c_gdat & 1)) == 1)  /* SCL_LO => SCL_HI */
    { 
      i2c_stat = 0;
      i2c_clck++;

      if((i2c_clck % 9) == 8 && (i2c_gdir & 2))
      {
        i2cout(1, ++i2c_bytn);
        i2c_data++;
      }
    }
    else if((des4 == 0xF005A028 || des4 == 0xF005A02C) && val == 2 && i2c_stat == 1)
    {
      *i2c_data |= (i2c_gdat & 2 ? 1 : 0) << (7 - i2c_clck % 9);
      i2c_stat = 0;
    }
  }
  else
  {
    /*    SCL_HI      +    output(SDA)   +   read(GPIOA) */
    if((i2c_gdat & 1) && !(i2c_gdir & 2) && des4 == 0xF005A020)
    {
      if((~i2c_dat[0] & 1) && ((i2c_clck-1) % 9) == 8) /* write to i2c slave device */
        *ptr &= ~2; /* set ACK bit */

      if(( i2c_dat[0] & 1) && ((i2c_clck-1) % 9) != 8) /* read from i2c slave device */
      {
        if(i2c_addr[0] == 0x10 && i2c_addr[1] == 0x02) /* INT1, INT2, INT3 */
          if((i2c_ints[(i2c_clck-1)/9-1] << (i2c_clck-1) % 9) & 0x80) *ptr |= 2;
          else                                                        *ptr &=~2;

        if(i2c_addr[0] == 0x10 && i2c_addr[1] == 0x0A) /* RTCSC */
          if((i2c_time[(i2c_clck-1)/9-1] << (i2c_clck-1) % 9) & 0x80) *ptr |= 2;
          else                                                        *ptr &=~2;

        if(i2c_addr[0] == 0x10 && i2c_addr[1] == 0x30) /* PCF5060X_ADCS1 */
        {
          if(i2c_chan == 0x11)
            if((i2c_xpos[(i2c_clck-1)/9-1] << (i2c_clck-1) % 9) & 0x80) *ptr |= 2;
            else                                                        *ptr &=~2;

          if(i2c_chan == 0x13)
            if((i2c_ypos[(i2c_clck-1)/9-1] << (i2c_clck-1) % 9) & 0x80) *ptr |= 2;
            else                                                        *ptr &=~2;

          if(i2c_chan == 0x1d)
            if((i2c_xpos[(i2c_clck-1)/9-1] << (i2c_clck-1) % 9) & 0x80) *ptr |= 2;
            else                                                        *ptr &=~2;
        }

        if(i2c_addr[0] == 0x10 && i2c_addr[1] == 0x32) /* PCF5060X_ADCS3 */
        {
          if(i2c_chan == 0x1d)
            if((i2c_ypos[(i2c_clck-1)/9-1] << (i2c_clck-1) % 9) & 0x80) *ptr |= 2;
            else                                                        *ptr &=~2;
        }

        if(i2c_addr[0] == 0x10 && i2c_addr[1] == 0x31) /* PCF5060X_ADCS2 */
          if((i2c_adcs2 << (i2c_clck-1) % 9) & 0x80) *ptr |= 2;
          else                                       *ptr &=~2;

        if(i2c_addr[0] == 0x10 && i2c_addr[1] == 0x2e) /* PCF5060X_ADCC1 (bit7:pen down) */
          if((i2c_pen << (i2c_clck-1) % 9) & 0x80) *ptr |= 2;
          else                                     *ptr &=~2;

        i2c_dat[(i2c_clck-1)/9] |= (*ptr & 2 ? 1 : 0) << (7 - (i2c_clck-1) % 9);
      }
    }
  }

  return *ptr;
}

ULONG read_write(ULONG des, ULONG typ, ULONG val)
{
  ULONG *ptr, des4;
  char  name[128];

       if(des >= DRAM_ADDR && des < DRAM_ADDR + DRAM_SIZE)  return loadstore(dram + ((des & 0x1ffffff) >> 2), typ, val);
  else if(des >= IRAM_ADDR && des < IRAM_ADDR + IRAM_SIZE)  return loadstore(iram + ((des - IRAM_ADDR) >> 2), typ, val);
  else if(des >= CACH_ADDR && des < CACH_ADDR + CACH_SIZE)  return loadstore(cach + ((des - CACH_ADDR) >> 2), typ, val);
  else if(des >= EEPR_ADDR && des < EEPR_ADDR + EEPR_SIZE)  return loadstore(eepr + ((des - EEPR_ADDR) >> 2), typ, val);

  des4 = des & ~3; /* 4byte pointer align */
  ptr = get_register(des4);

  if(des >= 0xF0053000 && des < 0xF0054000) emu_nand_nfc(des4, ptr, typ, val);
  else                                      emu_cpu_cop (des4, ptr, typ, val);

  val = loadstore(ptr, typ, val);

  if(des4 >= 0xF005A020 && des4 < 0xF005A030)
    val = emu_i2c(des4, ptr, typ, val);

  /* filter for logging, here: DMA and ATA related */
  /*                      TIREQ                  CREQ                  MIRQ                  VAIRQ                  IEN  */
//if(regfile && des4 != 0xf3003060 && des4 != 0xf3001004 && des4 != 0xf3001028 && des4 != 0xf3001080 && des4 != 0xf3001000)
  if(regfile && (des4 == 0xF005A020 || des4 == 0xF005A024 || des4 == 0xF005A028 || des == 0xF005A02c))
    fprintf(regfile, "%07x %08x +%s %08x %s-%08x\n", iii, pc, format_stg(get_symbol(des4, name), 15), des4, rdwrstg[typ], val);

  if(accfile && typ < 7)
  {
    ULONG i = mptr[search_register(des4)];

    if(bptr[i] < 512)
    switch(typ)
    { case WRITE10:
      case WRITE11:
      case WRITE12:
      case WRITE13: *((UCHAR *)&idat[i][bptr[i]] + ((des & 3) >> 2)) = (UCHAR )val; bptr[i]++; break;
      case WRITE22:
      case WRITE20: *((USHORT*)&idat[i][bptr[i]] + ((des & 3) >> 1)) = (USHORT)val; bptr[i]++; break;
      case WRITE40: *((ULONG *)&idat[i][bptr[i]] + ((des & 3) >> 0)) = (ULONG )val; bptr[i]++; break;
    }
  }

  return val;
}

void switch_cpu_mode(CORE *c, ULONG mode)
{
  ULONG ii;

  if(c->old_mode != mode)
  {
    switch(mode)
    {
      case 0x1F: /* system */
      case 0x10: for(ii=13; ii<15; ii++)  c->reg[ii] = &c->rg[13-13+ii]; c->spsr = NULL;             break; /* user */
      case 0x11: for(ii= 8; ii<15; ii++)  c->reg[ii] = &c->rg[17- 8+ii]; c->spsr = (PSR*)&c->rg[24]; break; /* fiq */
      case 0x12: for(ii=13; ii<15; ii++)  c->reg[ii] = &c->rg[25-13+ii]; c->spsr = (PSR*)&c->rg[27]; break; /* irq */
      case 0x13: for(ii=13; ii<15; ii++)  c->reg[ii] = &c->rg[28-13+ii]; c->spsr = (PSR*)&c->rg[30]; break; /* supervisor */
      case 0x17: for(ii=13; ii<15; ii++)  c->reg[ii] = &c->rg[31-13+ii]; c->spsr = (PSR*)&c->rg[33]; break; /* abort */
      case 0x1B: for(ii=13; ii<15; ii++)  c->reg[ii] = &c->rg[34-13+ii]; c->spsr = (PSR*)&c->rg[36]; break; /* undefined */
      default: SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: Mode undef");
    }
    c->old_mode = c->cpsr->M;
    c->cpsr->M = mode;
  }
}

void switch_to_interrupt(CORE *c, ULONG vector)
{
  PSR   psr = *c->cpsr;
  ULONG mode;

  switch(vector)
  {
  case 0x00: mode = 0x13; break; /* reset       */
  case 0x04: mode = 0x1b; break; /* undefined   */
  case 0x08: mode = 0x13; break; /* swinterrupt */
  case 0x0c: mode = 0x17; break; /* abort (code)*/
  case 0x10: mode = 0x17; break; /* abort (data)*/
  case 0x14: mode = 0x13; break; /* reserved    */
  case 0x18: mode = 0x12; break; /* irq         */
  case 0x1c: mode = 0x11; break; /* fiq         */
  }

  switch_cpu_mode(c, mode);
  *c->spsr      = psr;
  c->irq_return = c->rg[15];
  *c->reg[14]   = c->rg[15] + (psr.T ? 0 : 4);
  c->rg[15]     = vector;
  c->cpsr->T    = 0;
  c->cpsr->I    = 1;
}

void switch_from_interrupt(CORE *c)
{
  *c->cpsr = *c->spsr;
  switch_cpu_mode(c, c->cpsr->M);
  c->irq_return = MAGIC;
}

int last_instr_moved_pc(CORE *c)
{
  if((c->opcode & 0x0e108000) == 0x08108000  /* ldm  x {pc} */
  || (c->opcode & 0x0fe0f000) == 0x0040f000  /* sub  pc, x  */
  || (c->opcode & 0x0fe0f000) == 0x0240f000  /* subs pc, x  */
  || (c->opcode & 0x0fe0f000) == 0x0060f000  /* rsb  pc, x  */
  || (c->opcode & 0x0fe0f000) == 0x0260f000  /* rsbs pc, x  */
  || (c->opcode & 0x0fe0f000) == 0x0080f000  /* add  pc, x  */
  || (c->opcode & 0x0fe0f000) == 0x0280f000  /* adds pc, x  */
  || (c->opcode & 0x0fe0f000) == 0x00c0f000  /* sbc  pc, x  */
  || (c->opcode & 0x0fe0f000) == 0x02c0f000  /* sbcs pc, x  */
  || (c->opcode & 0x0fe0f000) == 0x00e0f000  /* rsc  pc, x  */
  || (c->opcode & 0x0fe0f000) == 0x02e0f000  /* rscs pc, x  */
  || (c->opcode & 0x0fe0f000) == 0x01a0f000  /* mov  pc, x  */
  || (c->opcode & 0x0fe0f000) == 0x03a0f000) /* movs pc, x  */
    return 1;
  else
    return 0;
}

/******************** exception vector table *********************
* 00 Reset Supervisor
* 04 Undefined instruction Undefined
* 08 Software interrupt Supervisor
* 0C Abort (prefetch) Abort
* 10 Abort (data) Abort
* 14 Reserved Reserved
* 18 IRQ IRQ
* 1C FIQ FIQ
******************** exception vector table *********************/
void trigger_interrupts(CORE *c)
{
  if(tim1 == 0 && (*c->int_en & TIMER0_IRQ_MASK))
  { c->int_pending = 1 << (*vn_irq = 6); *m_irq = 0x40;
    *va_irq = 6 << 2;
    *tim_req = 0x101;
  }
  if(tim2 == 0 && (*c->int_en & TIMER0_IRQ_MASK))
  { c->int_pending = 1 << (*vn_irq = 6); *m_irq = 0x40;
    *va_irq = 6 << 2;
    *tim_req = 0x404;
  }

  if(c->rg[15] == c->irq_return && last_instr_moved_pc(c)) // just returned from interrupt?
  {
    switch_from_interrupt(c);
  }
  else
  if(c->cpsr->I == 0)
  {
    switch(c->int_pending)
    {
      case 0x00000000: break;

      case 0x00000040: /* TIMER0 interrupt */
      if(c->irq_return != MAGIC)
        putchar(8);
      c->int_pending = 0;
      switch_to_interrupt(c, 0x18);
      return;

      case 0x00000002: /* TIMER2 interrupt */
      *c->int_stat   = c->int_pending;
      c->int_pending = 0;
      switch_to_interrupt(c, 0x18);
      return;

      case 0x00000010: /* MBOX interrupt */
      *c->int_stat   = c->int_pending;
      c->int_pending = 0;
      switch_to_interrupt(c, 0x18);
      return;

      case 0x00000200: /* forced interrupt (sansa special: after DMA interrupt) */
      *c->int_stat   = c->int_pending;
      c->int_pending = 0;
      switch_to_interrupt(c, 0x18);
      return;

      /* TODO: let software do this */
      case 0x04000000: /* DMA interrupt: sansa special: force interrupt 0x200 */
      *c->int_stat   = c->int_pending;
      c->int_pending = 0x200;
      switch_to_interrupt(c, 0x18);
      return;

      default: SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: multi interrupts");
    }

    if(c->cpsr->T && ((c->opcode & 0xff00) == 0xdf00)) // swi interrupt
    { switch_to_interrupt(c, 0x08);
      return;
    }
  }
}

void load_eeprom(char *fname)
{
  FILE *fil = fopen(fname, "rb");
  if(fil)
  {
    fread(eepr, 0x2000, 1, fil);
    fclose(fil);
  }
}

void patch_screen(void);
char txt[256];

unsigned short arm5ext[0x100] = {
0xa000,0x0000,0xa000,0x0000,0xa000,0x0000,0xa000,0x0000,0xa000,0x0000,0xa000,0x0000,0xa000,0x0000,0xa000,0x0000,
0xf420,0x0000,0xf520,0x0000,0xf520,0x0000,0xf420,0x0000,0xa000,0x0000,0xa000,0x0000,0xa000,0x0000,0xa000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000 };

DWORD WINAPI RunCode(void *pRun)
{
  int   j;
  ULONG iend, sz, ret, op, usec=0;
//char  name[64], *executable = "eeprom.bin";
  char  name[64], *executable = "D:\\d2.bin";
//char  name[64], *executable = "C:\\work\\cowond2\\rockbox.bin";
  char            *mapfile    = "cowond2_orig.map"; //"sansa.map";
  int   evt_trigger = 1;

  load_eeprom("D:\\eeprom.bin");
  init_asm_buffer(asmbuf+0, "disassembly_cpu.txt");

  cpu.irq_return = MAGIC;
  cpu.id = 0x55;
  iptr[nptr++] = 0xfffffffc; // initialize register mapping

  parse_map_file(mapfile);
  parse_head_file("tcc780x.h");
  fil = fopen(executable, "rb");
  if(!fil)
  { PostMessage(hWnd, WM_COMMAND, 1, (LPARAM)"Firmware file not found"); *(DWORD*)pRun = 2; return 0; }
  fseek(fil, 0, SEEK_END); sz = ftell(fil);
  fseek(fil, 0, SEEK_SET); fread(dram, sz, 1, fil);
  fclose(fil);

  for(sz=0; sz<rgs_ind; sz++)
    GETUL(rgs_entry[sz].fptr); /* allocate space for the defined registers */

  cpu.rg[15]   = DRAM_ADDR;// + 0x64;
  cpu.cpsr     = (PSR*)&cpu.rg[16];
  /*                   IRQ    FIQ   THMB   SUPER */
  *(ULONG*)cpu.cpsr = 0x80 | 0x40 | 0x00 | 0x13; // set supervisor, enable irq,firq
  for(j=0; j<16; j++) cpu.reg[j] = &cpu.rg[j];

  atafile = fopen("ata.txt", "w");
  offfile = fopen("off.txt", "w");
  regfile = fopen("reg.txt", "w");
  accfile = fopen("accessed2.txt", "w");

  iend = 0x7FFFFFFF; /* the loop end is important for the instruction dump */

  for(iii=0; *(DWORD*)pRun && iii<iend; iii++)
  {
    if(++usec >= NUMINSTR_PER_USEC) { usec = 0;  (*usec_timer)++; }
    if(++tim1 >= timer1_limit)        tim1 = 0;
    if(++tim2 >= timer2_limit)        tim2 = 0;

    trigger_interrupts(&cpu);

    if(cpu.cpsr->T) /* thumb code here: not used by CowonD2 fw */
    {
      cpu.opcode = GETUS(pc=cpu.rg[15]);  cpu.rg[15]+=2; // get instruction & increment pc

      switch_cpu_mode(&cpu, cpu.cpsr->M);

      ctrue = 1;
      base0 = MAGIC;
      switch(cpu.opcode >> 11)
      {
        case  0: case 1: case 2:             ret = move_reg        (&cpu); break;
        case  3:                             ret = add_subtract    (&cpu); break;
        case  4: case 5: case 6: case 7:     ret = opcode_imm      (&cpu); break;
        case  8: if(cpu.opcode&0x400)        ret = hireg_ops_bx    (&cpu);
          else                               ret = alu_operation   (&cpu); break;
        case  9:                             ret = pc_relative_load(&cpu); break;
        case 10: case 11:                    ret = load_store      (&cpu); break;
        case 12: case 13: case 14: case 15:  ret = ls_immediate    (&cpu); break;
        case 16: case 17:                    ret = ls_halfword     (&cpu); break;
        case 18: case 19:                    ret = sp_relative_ls  (&cpu); break;
        case 20: case 21:                    ret = load_address    (&cpu); break;
        case 22: case 23:
          if((cpu.opcode & 0x0f00) == 0x0000)ret = sp_add_offset   (&cpu);
          else                               ret = push_pop_regs   (&cpu); break;
        case 24: case 25:                    ret = ls_multiple     (&cpu); break;
        case 26: case 27:                    ret = cond_branch     (&cpu); break;
        case 28:                             ret = uncond_branch   (&cpu); break;
        case 29:                             ret = undefined       (&cpu); break;
        case 30: case 31:                    ret = long_branch_link(&cpu); break;
      }

      if(iii + 500000 >= iend)
      {
        dis_thumb(&cpu, pc, cpu.stg);
        if(ctrue == 0)        sprintf(cpu.stg, "%s (ignored)", cpu.stg);
        else
        { if(ret   != MAGIC)  sadjust(cpu.stg, 49, ret);
          if(base0 != MAGIC)  sprintf(cpu.stg, "%s	[%s]", cpu.stg, get_symbol(base0, name));
        }
        check_for_loop(asmbuf+j, pc, iii, cpu.stg);
      }
    }
    else  /* ARM 32bit code here */
    {
      cpu.opcode = op = GETUL(pc=cpu.rg[15]);  cpu.rg[15]+=4; // get instruction & increment pc

      if(arm5ext[(op >> 20) & 0xff] & (1 << ((op >> 4) & 15)))
        SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"armv5 instruction");

      switch_cpu_mode(&cpu, cpu.cpsr->M);

      base0 = MAGIC;
      if( cond_true(&cpu, cpu.opcode>>28) )
      {
        switch((cpu.opcode >> 24) & 15)
        {
          case  0: case  1: case 2: case 3:   ret = opcode(&cpu);          break;
          case  4: case  5: case 6: case 7:   ret = single_transfer(&cpu); break;
          case  8: case  9:                   ret = multip_transfer(&cpu); break;
          case 10: case 11:                   ret = branch(&cpu);          break;
          case 12: case 13: case 14: case 15: if((cpu.opcode & 0x0fffffff) == 0xe17ff7a) cpu.cpsr->Z=1; // test,clean,invalidate DCache
                                              ret = MAGIC;                 break;
        }
      }

      if(iii + 500000 >= iend)
      {
        dis_asm(&cpu, pc, cpu.stg);
        if(ctrue == 0)        sprintf(cpu.stg, "%s (skip)", cpu.stg);
        else
        { if(ret   != MAGIC)  sadjust(cpu.stg, 49, ret);
          if(base0 != MAGIC)  sprintf(cpu.stg, "%s	[%s]", cpu.stg, get_symbol(base0, name));
        }

        if((cpu.opcode & 0x0fff0000) == 0x028f0000)
        /* check: "add/sub rn, pc,0x..." => usually pointer to string */
        if(((cpu.opcode & 0x0fff0000) == 0x028f0000) || ((cpu.opcode & 0x0fff0000) == 0x024f0000))
        {
          int i = ret & 0xffffff;

          if(i < sz)
          {
            memcpy(txt, (char*)dram+i, 255);
            while(txt[strlen(txt)-1] == 10 || txt[strlen(txt)-1] == 13) txt[strlen(txt)-1] = 0;
            sprintf(cpu.stg, "%s < \047%s\047", cpu.stg, txt);
          }
        }

        check_for_loop(asmbuf+j, pc, iii, cpu.stg);
      }
    }

    if((iii & 0xffff) == 0) /* periodical display update */
      SendMessage(hWnd, WM_COMMAND, 0, 0);
  }

  if(accfile)
  {
    ULONG i, j;
    for(i=1; i<256 && iptr[i]; i++)
    {
      if(aptr[i])
      {
        fprintf(accfile, "%s %08x: ", format_stg(get_symbol(iptr[i], name), 20), iptr[i]);
        for(j=0; j<bptr[i]; j++)
          fprintf(accfile, "%08x,", idat[i][j]);
        fprintf(accfile, "\n");
      }
    }
  }

  flush_loop_end(asmbuf+0, &cpu, iii);

  if(atafile) fclose(atafile);
  if(offfile) fclose(offfile);
  if(regfile) fclose(regfile);
  if(accfile) fclose(accfile);
/*
  { FILE *fil = fopen("wrdata.bin", "wb");
    fwrite(&wrnum, sizeof(wrnum), 1, fil);
    fwrite(&wrbin, sizeof(wrbin), 1, fil);
    fclose(fil);
  }
*/
  PostMessage(hWnd, WM_COMMAND, 1, (LPARAM)"Sniffing stopped");
  *(DWORD*)pRun = 2;
  ExitThread(0);
  return 0;
}
