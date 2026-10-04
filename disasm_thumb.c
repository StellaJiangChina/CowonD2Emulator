#include "cowond2.h"

static char *cond[16] = { "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le",   "", "nv" };
static char *regs[16] = { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "sl", "fp", "ip", "sp", "lr", "pc" };

/*****************************************************************
15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
 0  0  0  -Op- ---Offset5---- ---Rs--- ---Rd--- Move shifted register
 0  0  0  1  1  I Op Rn/offs3 ---Rs--- ---Rd--- Add/subtract
 0  0  1  -Op- ---Rd--- -------Offset8--------- Move/compare/add/subtract immediate
 0  1  0  0  0  0 -----Op---- ---Rs--- ---Rd--- ALU operations
 0  1  0  0  0  1  -Op- H1 H2 --Rs/Hs- --Rd/Hd- Hi register operations/branch exchange
 0  1  0  0  1 ---Rd--- --------Word8---------- PC-relative load
 0  1  0  1  L  B  0 ---Ro--- ---Rb--- ---Rd--- Load/store with register offset
 0  1  0  1  H  S  1 ---Ro--- ---Rb--- ---Rd--- Load/store sign-extended byte/halfword
 0  1  1  B  L ----Offset5--- ---Rb--- ---Rd--- Load/store with immediate offset
 1  0  0  0  L ----Offset5--- ---Rb--- ---Rd--- Load/store halfword
 1  0  0  1  L ---Rd--- --------Word8---------- SP-relative load/store
 1  0  1  0 SP ---Rd--- --------Word8---------- Load address
 1  0  1  1  0  0  0  0  S -----SWord7--------- Add offset to stack pointer
 1  0  1  1  L  1  0  R --------Rlist---------- Push/pop registers
 1  1  0  0  L ---Rb--- --------Rlist---------- Multiple load/store
 1  1  0  1 ---Cond---- -------Soffset8-------- Conditional branch
 1  1  0  1  1  1  1  1 --------Value8--------- Software Interrupt
 1  1  1  0  0  -----------Offset11------------ Unconditional branch
 1  1  1  1  H  -----------Offset11------------ Long branch with link
*********************************************************************/
/* 0  0  0  -Op- ---Offset5---- ---Rs--- ---Rd--- Move shifted register */
void disasm_move_reg(ULONG val, char *stg)
{
  static char *ops[] = { "lsl", "lsr", "asr" };
  sprintf(stg+strlen(stg), "%s 	%s, %s, #%d", ops[(val>>11)&3], regs[val&7], regs[(val>>3)&7], (val>>6)&31);
}

/* 0  0  0  1  1  I Op Rn/offs3 ---Rs--- ---Rd--- Add/subtract */
void disasm_add_subtract(ULONG val, char *stg)
{
  static char *ops[] = { "add", "sub" };

  if(val & 0x400) // immediate
    if((val>>6)&7)
      sprintf(stg+strlen(stg), "%s 	%s, %s, #%d", ops[(val>>9)&1], regs[val&7], regs[(val>>3)&7], (val>>6)&7);
    else
      sprintf(stg+strlen(stg), "mov 	%s, %s", regs[val&7], regs[(val>>3)&7]);
  else
    sprintf(stg+strlen(stg), "%s 	%s, %s, %s", ops[(val>>9)&1], regs[val&7], regs[(val>>3)&7], regs[(val>>6)&7]);
}

/* 0  0  1  -Op- ---Rd--- -------Offset8--------- Move/compare/add/subtract immediate */
void disasm_opcode_imm(ULONG val, char *stg)
{
  static char *ops[] = { "mov", "cmp", "add", "sub" };

  sprintf(stg+strlen(stg), "%s 	%s, #%d", ops[(val>>11)&3], regs[(val>>8)&7], val&255);
}

/* 0  1  0  0  0  0 -----Op---- ---Rs--- ---Rd--- ALU operations */
void disasm_alu_operation(ULONG val, char *stg)
{
  static char *ops[] = {"and","eor","lsl","lsr","asr","adc","sbc","ror","tst","neg","cmp","cmn","orr","mul","bic","mvn"};

  sprintf(stg+strlen(stg), "%s 	%s, %s", ops[(val>>6)&15], regs[val&7], regs[(val>>3)&7]);
}

/* 0  1  0  0  0  1  -Op- H1 H2 --Rs/Hs- --Rd/Hd- Hi register operations/branch exchange */
void disasm_hireg_ops_bx(ULONG val, char *stg)
{
  switch((val>>6) & 15)
  {
  case  1: sprintf(stg+strlen(stg), "add 	%s, %s", regs[(val&7)  ], regs[((val>>3)&7)+8]); break;
  case  2: sprintf(stg+strlen(stg), "add 	%s, %s", regs[(val&7)+8], regs[((val>>3)&7)  ]); break;
  case  3: sprintf(stg+strlen(stg), "add 	%s, %s", regs[(val&7)+8], regs[((val>>3)&7)+8]); break;

  case  5: sprintf(stg+strlen(stg), "cmp 	%s, %s", regs[(val&7)  ], regs[((val>>3)&7)+8]); break;
  case  6: sprintf(stg+strlen(stg), "cmp 	%s, %s", regs[(val&7)+8], regs[((val>>3)&7)  ]); break;
  case  7: sprintf(stg+strlen(stg), "cmp 	%s, %s", regs[(val&7)+8], regs[((val>>3)&7)+8]); break;

  case  9: sprintf(stg+strlen(stg), "mov 	%s, %s", regs[(val&7)  ], regs[((val>>3)&7)+8]); break;
  case 10: sprintf(stg+strlen(stg), "mov 	%s, %s", regs[(val&7)+8], regs[((val>>3)&7)  ]); break;
  case 11: sprintf(stg+strlen(stg), "mov 	%s, %s", regs[(val&7)+8], regs[((val>>3)&7)+8]); break;

  case 12: sprintf(stg+strlen(stg), "bx	%s", regs[((val>>3)&7)  ]); break;
  case 13: sprintf(stg+strlen(stg), "bx	%s", regs[((val>>3)&7)+8]); break;

  default: sprintf(stg+strlen(stg), "undefined"); break;
  }
}

/* 0  1  0  0  1 ---Rd--- --------Word8---------- PC-relative load */
void disasm_pc_relative_load(ULONG val, char *stg)
{
  sprintf(stg+strlen(stg), "ldr 	r%d, [pc, #%d]", (val>>8)&7, 4*(val & 0xff));
}

/* 0  1  0  1  L  B  0 ---Ro--- ---Rb--- ---Rd--- Load/store with register offset */
/* 0  1  0  1  H  S  1 ---Ro--- ---Rb--- ---Rd--- Load/store sign-extended byte/halfword */
void disasm_load_store(ULONG val, char *stg)
{
  if(val & 0x200)   /* Load/store sign-extended byte/halfword */
    if(val & 0x800) /* load */
      if(val & 0x400) sprintf(stg+strlen(stg), "ldrh	r%d, [r%d, r%d]", val&7, (val>>3)&7, (val>>6)&7);
      else            sprintf(stg+strlen(stg), "ldsh	r%d, [r%d, r%d]", val&7, (val>>3)&7, (val>>6)&7);
    else
      if(val & 0x400) sprintf(stg+strlen(stg), "ldsb	r%d, [r%d, r%d]", val&7, (val>>3)&7, (val>>6)&7);
      else            sprintf(stg+strlen(stg), "strh	r%d, [r%d, r%d]", val&7, (val>>3)&7, (val>>6)&7);
  else /* Load/store with register offset */
    if(val & 0x800) /* load */
      if(val & 0x400) sprintf(stg+strlen(stg), "ldrb	r%d, [r%d, r%d]", val&7, (val>>3)&7, (val>>6)&7);
      else            sprintf(stg+strlen(stg), "ldr 	r%d, [r%d, r%d]", val&7, (val>>3)&7, (val>>6)&7);
    else
      if(val & 0x400) sprintf(stg+strlen(stg), "strb	r%d, [r%d, r%d]", val&7, (val>>3)&7, (val>>6)&7);
      else            sprintf(stg+strlen(stg), "str 	r%d, [r%d, r%d]", val&7, (val>>3)&7, (val>>6)&7);
}

/* 0  1  1  B  L ----Offset5--- ---Rb--- ---Rd--- Load/store with immediate offset */
void disasm_ls_immediate(ULONG val, char *stg)
{
  if(val & 0x800)
    if(val & 0x1000) sprintf(stg+strlen(stg), "ldrb	r%d, [r%d, #%d]", val&7, (val>>3)&7, 1*((val>>6)&31));
    else             sprintf(stg+strlen(stg), "ldr 	r%d, [r%d, #%d]", val&7, (val>>3)&7, 4*((val>>6)&31));
  else
    if(val & 0x1000) sprintf(stg+strlen(stg), "strb	r%d, [r%d, #%d]", val&7, (val>>3)&7, 1*((val>>6)&31));
    else             sprintf(stg+strlen(stg), "str 	r%d, [r%d, #%d]", val&7, (val>>3)&7, 4*((val>>6)&31));
}

/* 1  0  0  0  L ----Offset5--- ---Rb--- ---Rd--- Load/store halfword */
void disasm_ls_halfword(ULONG val, char *stg)
{
  if(val & 0x800)  sprintf(stg+strlen(stg), "ldrh	r%d, [r%d, #%d]", val&7, (val>>3)&7, 2*((val>>6)&31));
  else             sprintf(stg+strlen(stg), "strh	r%d, [r%d, #%d]", val&7, (val>>3)&7, 2*((val>>6)&31));
}

/* 1  0  0  1  L ---Rd--- --------Word8---------- SP-relative load/store */
void disasm_sp_relative_ls(ULONG val, char *stg)
{
  if(val & 0x800)  sprintf(stg+strlen(stg), "ldr 	r%d, [sp, #%d]", (val>>8)&7, 4*(val&255));
  else             sprintf(stg+strlen(stg), "str 	r%d, [sp, #%d]", (val>>8)&7, 4*(val&255));
}

/* 1  0  1  0 SP ---Rd--- --------Word8---------- Load address */
void disasm_load_address(ULONG val, char *stg)
{
  if(val & 0x800)  sprintf(stg+strlen(stg), "add 	r%d, sp, #%d", (val>>8)&7, 4*(val&255));
  else             sprintf(stg+strlen(stg), "add 	r%d, pc, #%d", (val>>8)&7, 4*(val&255));
}

/* 1  0  1  1  0  0  0  0  S -----SWord7--------- Add offset to stack pointer */
void disasm_sp_add_offset(ULONG val, char *stg)
{
  if(val & 0x80)  sprintf(stg+strlen(stg), "sub 	sp, #%d", 4*(val&127));
  else            sprintf(stg+strlen(stg), "add 	sp, #%d", 4*(val&127));
}

/* 1  0  1  1  L  1  0  R --------Rlist---------- Push/pop registers */
void disasm_push_pop_regs(ULONG val, char *stg)
{
  int i;

  if(val & 0x800)  sprintf(stg+strlen(stg), "pop 	{");
  else             sprintf(stg+strlen(stg), "push	{");

  for(i=0;i<8;i++) if(val&(1<<i)) sprintf(stg+strlen(stg), "r%d, ", i);

  if(val & 0x100) /* add lr/pc */
    if(val & 0x800) /* load */ sprintf(stg+strlen(stg), "pc}");
    else            /* stor */ sprintf(stg+strlen(stg), "lr}");
  else
    sprintf(stg+strlen(stg)-2, "}");
}

/* 1  1  0  0  L ---Rb--- --------Rlist---------- Multiple load/store */
void disasm_ls_multiple(ULONG val, char *stg)
{
  int   i, n=(val>>8)&7, cnt=0;

  if(val & 0x800)  sprintf(stg+strlen(stg), "ldmia	%s!,{", regs[(val>>8)&7]);
  else             sprintf(stg+strlen(stg), "stmia	%s!,{", regs[(val>>8)&7]);

  for(i=0;i<8;i++) if(val&(1<<i)) sprintf(stg+strlen(stg), "%s, ", regs[i]);
  sprintf(stg+strlen(stg)-2, "}");
}

/* 1  1  0  1  1  1  1  1 --------Value8--------- Software Interrupt */
/* 1  1  0  1 ---Cond---- -------Soffset8-------- Conditional branch */
void disasm_cond_branch(ULONG pc, ULONG val, char *stg)
{
  if(((val>>8) & 15) == 15)
    sprintf(stg+strlen(stg), "swi 	0x%02x", val & 255);
  else
    sprintf(stg+strlen(stg), "b%s	0x%08x", cond[(val>>8)&15], pc+4+((LONG)(val<<24)>>23));
}

/* 1  1  1  0  0  -----------Offset11------------ Unconditional branch */
void disasm_uncond_branch(ULONG pc, ULONG val, char *stg)
{
  sprintf(stg+strlen(stg), "b	0x%08x", pc+4+((LONG)(val<<21)>>20));
}

void disasm_undefined(ULONG val, char *stg)
{
  sprintf(stg+strlen(stg), "undefined opcode thumb");
}

/* 1  1  1  1  H  -----------Offset11------------ Long branch with link */
void disasm_long_branch_link(ULONG pc, ULONG val, char *stg)
{
  ULONG off;

  if(val & 0x800)
  {
    off = (GETUS(pc-2) << 11) + (val & 0x7ff);
    sprintf(stg+strlen(stg), "bl	0x%08x", pc+2+((LONG)(off<<10)>>9));
  }
  else
    sprintf(stg+strlen(stg), "bl		->");
}


void dis_thumb(CORE *c, ULONG off, char *stg)
{
  ULONG val = c->opcode;

  sprintf(stg, "%c %8x:		%04x	", c->irq_return == MAGIC ? 'N' : 'I', off, val);

  switch(val >> 11)
  {
    case  0: case 1: case 2:            disasm_move_reg(val, stg);             break;
    case  3:                            disasm_add_subtract(val, stg);         break;
    case  4: case 5: case 6: case 7:    disasm_opcode_imm(val, stg);           break;
    case  8: if(val&0x400)              disasm_hireg_ops_bx(val, stg);
             else                       disasm_alu_operation(val, stg);        break;
    case  9:                            disasm_pc_relative_load(val, stg);     break;
    case 10: case 11:                   disasm_load_store(val, stg);           break;
    case 12: case 13: case 14: case 15: disasm_ls_immediate(val, stg);         break;
    case 16: case 17:                   disasm_ls_halfword(val, stg);          break;
    case 18: case 19:                   disasm_sp_relative_ls(val, stg);       break;
    case 20: case 21:                   disasm_load_address(val, stg);         break;
    case 22: case 23:
      if((val & 0x0f00) == 0x0000)      disasm_sp_add_offset(val, stg);
      else                              disasm_push_pop_regs(val, stg);        break;
    case 24: case 25:                   disasm_ls_multiple(val, stg);          break;
    case 26: case 27:                   disasm_cond_branch(off, val, stg);     break;
    case 28:                            disasm_uncond_branch(off, val, stg);   break;
    case 29:                            disasm_undefined(val, stg);            break;
    case 30: case 31:                   disasm_long_branch_link(off, val, stg);break;
  }
}
