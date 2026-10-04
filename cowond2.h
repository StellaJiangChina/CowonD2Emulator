#define STRICT
#include <windows.h>
#include <windowsx.h>
#pragma hdrstop
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#define WRITE10  0
#define WRITE11  1
#define WRITE12  2
#define WRITE13  3
#define WRITE20  4
#define WRITE22  5
#define WRITE40  6
#define READU10  7
#define READU11  8
#define READU12  9
#define READU13 10
#define READS10 11
#define READS11 12
#define READS12 13
#define READS13 14
#define READU20 15
#define READU22 16
#define READS20 17
#define READS22 18
#define READU40 19

#define XSIZE 320
#define YSIZE 240

#define    U64 unsigned __int64
#define    S64          __int64
#define  ULONG unsigned long
#define   LONG   signed long
#define USHORT unsigned short
#define  UCHAR unsigned char
#define MAGIC  0x1cbfedcb

typedef struct
{
  ULONG addrss[256];
  char  strings[256][120];
  FILE  *fil;
  ULONG ichk;
  ULONG ip;
  ULONG ips;
  ULONG ipstart;
  ULONG cnt;
} ASM_BUFFER;

typedef struct
{
  char  *fname;
  ULONG fptr;
} MAP_ENTRY;

typedef struct
{
  unsigned int M :  5;
  unsigned int T :  1;
  unsigned int F :  1;
  unsigned int I :  1;
  unsigned int r : 20;
  unsigned int V :  1;
  unsigned int C :  1;
  unsigned int Z :  1;
  unsigned int N :  1;
} PSR;

typedef struct
{
  char   stg[256];
  ULONG  irq_return;
  ULONG  id; // cpu:0x55, cop:0xaa
  ULONG  opcode;
  ULONG  int_pending;
  ULONG  *control;
  ULONG  *int_en;
  ULONG  *int_stat;
  ULONG  *ihi_stat;
  ULONG  *fiq_stat;
  ULONG  old_mode;
  ULONG  rg[37], *reg[16];
  PSR    *cpsr, *spsr;
} CORE;

extern HWND       hWnd;
extern CORE       cpu, cop;
extern ULONG      mapind;
extern ULONG      rgs_ind;
extern MAP_ENTRY  mapentry[2000];
extern MAP_ENTRY  rgs_entry[512];
extern USHORT     *lcd_buffer;
extern ULONG      base0; /* ldr/str address: exported for disassembler usage */
extern ULONG      ctrue; /* condition true : exported for disassembler usage */

extern ULONG cond_true       (CORE *c, ULONG val);
extern ULONG opcode          (CORE *c);
extern ULONG single_transfer (CORE *c);
extern ULONG multip_transfer (CORE *c);
extern ULONG branch          (CORE *c);
extern ULONG branchexchg     (CORE *c, ULONG val, int blx_instr);

extern ULONG move_reg        (CORE *c);
extern ULONG add_subtract    (CORE *c);
extern ULONG opcode_imm      (CORE *c);
extern ULONG alu_operation   (CORE *c);
extern ULONG hireg_ops_bx    (CORE *c);
extern ULONG pc_relative_load(CORE *c);
extern ULONG load_store      (CORE *c);
extern ULONG ls_immediate    (CORE *c);
extern ULONG ls_halfword     (CORE *c);
extern ULONG sp_relative_ls  (CORE *c);
extern ULONG load_address    (CORE *c);
extern ULONG sp_add_offset   (CORE *c);
extern ULONG push_pop_regs   (CORE *c);
extern ULONG ls_multiple     (CORE *c);
extern ULONG cond_branch     (CORE *c);
extern ULONG uncond_branch   (CORE *c);
extern ULONG undefined       (CORE *c);
extern ULONG long_branch_link(CORE *c);

extern DWORD  WINAPI RunCode(void *pRun);

extern void sadjust(char *stg, ULONG pos, ULONG ret);
extern char *get_symbol(ULONG addr, char *name);
extern char *format_stg(char *stg, ULONG stop);
extern void parse_map_file(char *fname);
extern void parse_head_file(char *fname);
extern void dis_asm  (CORE *c, ULONG off, char *stg);
extern void dis_thumb(CORE *c, ULONG off, char *stg);
extern void init_asm_buffer(ASM_BUFFER *asmbuf, char *fname);
extern void check_for_loop(ASM_BUFFER *asmbuf, ULONG pc, ULONG ind, char *stg);
extern void flush_loop_end(ASM_BUFFER *asmbuf, CORE *c, ULONG ind);
extern ULONG *get_register(ULONG des);
//extern ULONG *get_ptr(ULONG des, ULONG typ, ULONG val);
extern ULONG read_write(ULONG des, ULONG typ, ULONG val);
extern ULONG GETUL(ULONG des);
extern ULONG GETUS(ULONG des);
extern ULONG GETSS(ULONG des);
extern ULONG GETSB(ULONG des);
extern ULONG GETUB(ULONG des);
extern  void SETUL(ULONG des, ULONG val);
extern  void SETUS(ULONG des, ULONG val);
extern  void SETUB(ULONG des, ULONG val);

#define NUMINSTR_PER_USEC        1  // after 1 instructions increment the usec counter
#define CACH_ADDR       0xa0000080
#define CACH_SIZE           0x2000  //   8kB
#define IRAM_ADDR       0x00000000
#define IRAM_SIZE         0x200000  //   2MB
#define DRAM_ADDR       0x20000000
#define DRAM_SIZE        0x2000000  //  32MB
#define EEPR_ADDR       0xe0000000
#define EEPR_SIZE           0x2000  //   8kB
