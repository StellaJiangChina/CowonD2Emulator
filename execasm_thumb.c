#include "cowond2.h"
 
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

/* 0  0  0  1  1  0 Op --Rn3-- ---Rs--- ---Rd--- Add/subtract */
ULONG move_reg(CORE *c)
{
  ULONG val  = c->opcode;
  ULONG vals = *c->reg[(val >> 3) & 7];
  ULONG shft = (val >> 6) & 31;
  U64   res;

  if(shft == 0)
    switch(val >> 11) // 0=lsl, 1=lsr, 2=asr
    {
      case 0: res = (U64)vals | (c->cpsr->C ? 0x100000000L : 0); break;
      case 1: res = (vals   &   0x80000000  ? 0x100000000L : 0); break;
      case 2: res = (vals   &   0x80000000  ? 0x1ffffffffL : 0); break;
    }
  else
    switch(val >> 11) // 0=lsl, 1=lsr, 2=asr
    {
      case 0: res =   (U64)vals << shft; break;
      case 1: res = (((U64)vals << (33-shft)) & 0x100000000L) + (vals >> shft); break;
      case 2: res = (((U64)vals << (33-shft)) & 0x100000000L) + (((long)vals >> shft) & 0xffffffff); break;
    }

  c->cpsr->C = res & 0x100000000L ? 1 : 0;
  c->cpsr->Z = res & 0xffffffff   ? 0 : 1;
  c->cpsr->N = res & 0x80000000   ? 1 : 0;

  return *c->reg[val&7] = (ULONG)res;
}

/* 0  0  0  1  1  I Op Rn/offs3 ---Rs--- ---Rd--- Add/subtract */
ULONG add_subtract(CORE *c)
{
  ULONG val = c->opcode;
  ULONG ua  = *c->reg[(val >> 3) & 7];
  ULONG ub  = val & 0x400 ? (val >> 6) & 7 : *c->reg[(val >> 6) & 7];
   LONG ia  = (LONG)ua;
   LONG ib  = (LONG)ub;
    U64 res;
    S64 ir;

  if(val & 0x200) // sub
  { *c->reg[val&7] = (ULONG)(res = 0x100000000L + ua - ub); ir = (S64)ia - ib; }
  else
  { *c->reg[val&7] = (ULONG)(res =           (U64)ua + ub); ir = (S64)ia + ib; }

  c->cpsr->Z = res &  0xffffffff ? 0 : 1;
  c->cpsr->N = res &  0x80000000 ? 1 : 0;
  c->cpsr->C = res & 0x100000000 ? 1 : 0;
  c->cpsr->V = ((ir>>31) == 0) || ((ir>>31) == -1) ? 0 : 1;

  return (ULONG)res;
}

/* 0  0  1  -Op- ---Rd--- -------Offset8--------- Move/compare/add/subtract immediate */
ULONG opcode_imm(CORE *c)
{
  ULONG val = c->opcode;
  ULONG ua  = *c->reg[(val >> 8) & 7];
  ULONG ub  = val & 255;
   LONG ia  = (LONG)ua;
    U64 res;
    S64 ir;

  /* 0:mov, 1:cmp, 2:add, 3:sub */
  switch((val>>11) & 3)
  { case 0: *c->reg[(val>>8)&7] = (ULONG)(res = ub);                     ir = ub;           break;
    case 1:                              (res = 0x100000000L + ua - ub); ir = (S64)ia - ub; break;
    case 2: *c->reg[(val>>8)&7] = (ULONG)(res =           (U64)ua + ub); ir = (S64)ia + ub; break;
    case 3: *c->reg[(val>>8)&7] = (ULONG)(res = 0x100000000L + ua - ub); ir = (S64)ia - ub; break;
  }

  c->cpsr->Z = res &  0xffffffff ? 0 : 1;
  c->cpsr->N = res &  0x80000000 ? 1 : 0;
  c->cpsr->C = res & 0x100000000 ? 1 : 0;
  c->cpsr->V = ((ir>>31) == 0) || ((ir>>31) == -1) ? 0 : 1;

  return (ULONG)res;
}

/* 0  1  0  0  0  0 -----Op---- ---Rs--- ---Rd--- ALU operations
0 AND 1 EOR 2 LSL 3 LSR 4 ASR 5 ADC 6 SBC 7 ROR
8 TST 9 NEG a CMP b CMN c ORR d MUL e BIC f MVN */
ULONG alu_operation(CORE *c)
{
  ULONG        val = c->opcode;
  ULONG        ua, ub, op, des;
  S64          ir, ia, ib;
  unsigned S64 ur;

  des = (val>>0) &  7;
  op  = (val>>6) & 15;
  ua  = *c->reg[des];
  ub  = *c->reg[(val>>3) & 7];
  ia  = (LONG)ua;
  ib  = (LONG)ub;

  if((op > 1 && op < 5) || op == 7)
  {
    ub &= 0xff; /* use only low byte */
    if(ub == 0)
    { ur = (U64)ua | (c->cpsr->C ? 0x100000000L : 0);
      op = 16; /* skip operation */
    }
  }

  // TODO: correct flagging?
  switch(op)
  {
  case  0: ur = *c->reg[des] = ua & ub; break;
  case  1: ur = *c->reg[des] = ua ^ ub; break;

  case  2: ur = (ub > 32 ? 0 : ((U64)ua | (c->cpsr->C ? 0x100000000L : 0))) << ub;
           *c->reg[des] = (ULONG)(ir = ur); break;

  case  3: ur = (ub > 32 ? 0 : (((U64)ua<<32) | (c->cpsr->C ?  0x80000000L : 0))) >> ub;
           *c->reg[des] = (ULONG)(ir = ur = (ur >> 32) | (ur & 0x80000000 ? 0x100000000L : 0));  break;

  case  4: ir = (ub >= 32 ? (ua & 0x80000000 ? -1 : 0) : (((S64)ia<<32) | (c->cpsr->C ? 0x80000000 : 0))) >> ub;
           *c->reg[des] = (ULONG)(ir = ur = (ir >> 32) | (ir & 0x80000000 ? 0x100000000L : 0));  break;

  case  5: *c->reg[des] = (ULONG)(ur =           (U64)ua + ub + c->cpsr->C);     ir = (S64)ia + ib + c->cpsr->C;     break;
  case  6: *c->reg[des] = (ULONG)(ur = 0x100000000L + ua - ub + c->cpsr->C - 1); ir = (S64)ia - ib + c->cpsr->C - 1; break;

  case  7: if(ub == 32) *c->reg[des] = (ULONG)(ir = ur = (ua & 0x80000000 ? 0x100000000L : 0) | ua);
           else
            if(ub & 31) *c->reg[des] = (ULONG)(ir = ur = (ua << (32 - (ub&31))) + (ua >> (ub&31)) + (ua & (1 << ((ub&31)-1)) ? 0x100000000 : 0));
            else        *c->reg[des] = (ULONG)(ir = ur = ((U64)ua << 32) + (ua >> 1) + (c->cpsr->C << 31)); break;
  
  case  8:                        ur =                ua & ub;                     break;
  case  9: *c->reg[des] = (ULONG)(ur = 0x100000000L - ub);      ir = -ib;          break;
  case 10:                        ur = 0x100000000L + ua - ub;  ir = (S64)ia - ib; break;
  case 11:                        ur =           (U64)ua + ub;  ir = (S64)ia + ib; break;
  case 12: ur = *c->reg[des] =                        ua | ub;                     break;
  case 13: ur = *c->reg[des] =                        ua * ub;  ir = ur;           break;
  case 14: ur = *c->reg[des] =                        ua & ~ub;                    break;
  case 15: ur = *c->reg[des] =                       ~ub;                          break;
  }

  c->cpsr->Z = ur &  0xffffffff ? 0 : 1;
  c->cpsr->N = ur &  0x80000000 ? 1 : 0;
  c->cpsr->C = ur & 0x100000000 ? 1 : 0;

  switch(op)
  {
/* 5 ADC 6 SBC 7 ROR 9 NEG a CMP b CMN */
  case 5: case 6: case 9: case 10: case 11:
      c->cpsr->V = ((ir>>31) == 0) || ((ir>>31) == -1) ? 0 : 1;
  }
  return (ULONG)ur;
}

/* 0  1  0  0  0  1  -Op- H1 H2 --Rs/Hs- --Rd/Hd- Hi register operations/branch exchange */
ULONG hireg_ops_bx(CORE *c)
{
  ULONG val = c->opcode;
  ULONG cod =  (val>>6) & 15;
  ULONG rd  = ((val>>0) &  7) + (cod & 2 ? 8 : 0);
  ULONG rs  = ((val>>3) &  7) + (cod & 1 ? 8 : 0);
  ULONG vrd = *c->reg[rd]     + (rd == 15 ? 2 : 0);
  ULONG vrs = *c->reg[rs]     + (rs == 15 ? 2 : 0);
  U64   ur;
  S64   ir;

  switch(cod)
  {
  case  1: case  2: case  3:  ur = *c->reg[rd] = vrd + vrs; break;
  case  5: case  6: case  7:  ur = 0x100000000L + vrd - vrs;
                              ir = (S64)(LONG)vrd - (LONG)vrs;
                              c->cpsr->Z = ur & 0xffffffff ? 0 : 1;
                              c->cpsr->N = ur & 0x80000000 ? 1 : 0;
                              c->cpsr->V = ((ir>>31) == 0) || ((ir>>31) == -1) ? 0 : 1;
                              c->cpsr->C = ur & 0x100000000 ? 1 : 0; break;
  case  9: case 10: case 11:  ur = *c->reg[rd] = vrs;       break;
  case 12: case 13:           ur = branchexchg(c, vrs, 0);  break;
  default:                    ur = MAGIC;
  }

  return (ULONG)ur;
}

/* 0  1  0  0  1 ---Rd--- --------Word8---------- PC-relative load */
ULONG pc_relative_load(CORE *c)
{
  ULONG val = c->opcode;
  ULONG off = (val & 0xff) << 2;
  ULONG rd  = (val>>8) & 7;

  return *c->reg[rd] = GETUL(*c->reg[15] + 2 + off & ~3);
}

/* 0  1  0  1  H  S  1 ---Ro--- ---Rb--- ---Rd--- Load/store sign-extended byte/halfword */
/* 0  1  0  1  L  B  0 ---Ro--- ---Rb--- ---Rd--- Load/store with register offset        */
ULONG load_store(CORE *c)
{
  ULONG val = c->opcode;
  ULONG rd  =         (val>>0) & 7;
  ULONG bas = *c->reg[(val>>3) & 7];
  ULONG off = *c->reg[(val>>6) & 7];
  ULONG res;

  base0 = bas + off;

  if(val & 0x200)
  { /* Load/store sign-extended byte/halfword */
    if(val & 0x800) /* halfword */
      if(val & 0x400) *c->reg[rd] = res = GETSS(bas+off);
      else            *c->reg[rd] = res = GETUS(bas+off);
    else
      if(val & 0x400) *c->reg[rd] = res = GETSB(bas+off);
      else            SETUS(bas+off, res=*c->reg[rd]);
  }
  else
  { /* Load/store with register offset */
    if(val & 0x800) /* load */
      if(val & 0x400) *c->reg[rd] = res = GETUB(bas+off);
      else            *c->reg[rd] = res = GETUL(bas+off);
    else
      if(val & 0x400) SETUB(bas+off, res=*c->reg[rd]);
      else            SETUL(bas+off, res=*c->reg[rd]);
  }

  return res;
}

/* 0  1  1  B  L ----Offset5--- ---Rb--- ---Rd--- Load/store with immediate offset */
ULONG ls_immediate(CORE *c)
{
  ULONG val = c->opcode;
  ULONG rd  =         (val>>0) & 7;
  ULONG bas = *c->reg[(val>>3) & 7];
  ULONG off =         (val>>6) & 31;
  ULONG res;

  if(val & 0x800) /* load */
    if(val & 0x1000) *c->reg[rd] = res = GETUB(base0=bas+1*off);
    else             *c->reg[rd] = res = GETUL(base0=bas+4*off);
  else
    if(val & 0x1000) SETUB(base0=bas+1*off, res=*c->reg[rd]);
    else             SETUL(base0=bas+4*off, res=*c->reg[rd]);

  return res;
}

/* 1  0  0  0  L ----Offset5--- ---Rb--- ---Rd--- Load/store halfword */
ULONG ls_halfword(CORE *c)
{
  ULONG val = c->opcode;
  ULONG rd  =         (val>>0) & 7;
  ULONG bas = *c->reg[(val>>3) & 7];
  ULONG off =         (val>>6) & 31;
  ULONG res;

  if(val & 0x800)  *c->reg[rd] = res = GETUS(base0=bas+2*off);
  else             SETUS(base0=bas+2*off, res=*c->reg[rd]);

  return res;
}

/* 1  0  0  1  L ---Rd--- --------Word8---------- SP-relative load/store */
ULONG sp_relative_ls(CORE *c)
{
  ULONG val = c->opcode;
  ULONG rd  = (val>>8) & 7;
  ULONG bas = *c->reg[13];
  ULONG off = val & 255;
  ULONG res;

  if(val & 0x800)  *c->reg[rd] = res = GETUL(bas+4*off);
  else             SETUL(bas+4*off, res=*c->reg[rd]);

  return res;
}

/* 1  0  1  0 SP ---Rd--- --------Word8---------- Load address */
ULONG load_address(CORE *c)
{
  ULONG val = c->opcode;
  ULONG rd  = (val>>8) & 7;
  ULONG bas = val & 0x800 ? *c->reg[13] : (*c->reg[15] + 2 & ~2);
  ULONG off = val & 255;
  ULONG res;

  *c->reg[rd] = res = bas + 4*off;

  return res;
}

/* 1  0  1  1  0  0  0  0  S -----SWord7--------- Add offset to stack pointer */
ULONG sp_add_offset(CORE *c)
{
  ULONG val = c->opcode;

  return *c->reg[13] += (val & 0x80) ? -4*(val & 127) : 4*(val & 127);
}

/* 1  0  1  1  L  1  0  R --------Rlist---------- Push/pop registers */
ULONG push_pop_regs(CORE *c)
{
  int   i;
  ULONG val = c->opcode;
  ULONG r   = *c->reg[13];

  if(val & 0x100) /* add lr/pc */
    if(val & 0x800) /* load */
    { for(i=0;i<8;i++) if(val&(1<<i)) {*c->reg[i] = GETUL(r); r+=4;}
      *c->reg[15] = GETUL(r); r+=4;
    }
    else
    { SETUL(r-=4, *c->reg[14]);
      for(i=8; i--;  ) if(val&(1<<i)) SETUL(r-=4, *c->reg[i]);
    }
  else
    if(val & 0x800) /* load */
    { for(i=0;i<8;i++) if(val & (1<<i)) { *c->reg[i] = GETUL(r); r+=4;} }
    else
    { for(i=8; i--;  ) if(val & (1<<i)) { SETUL(r-=4, *c->reg[i]);    } }

  *c->reg[13] = r;

  return MAGIC;
}

/* 1  1  0  0  L ---Rb--- --------Rlist---------- Multiple load/store */
ULONG ls_multiple(CORE *c)
{
  ULONG val = c->opcode;
  int   i, n = (val>>8) & 7, cnt=0;
  ULONG r = *c->reg[n];

  for(i=0;i<8;i++)
    if(val & (1<<i))
    { if(cnt != 0 && i == n && !(val & 0x800)) // n included, but not first in stm command
        SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: danger stm");
      if(i == n) // n is included
        n = 8;
      cnt++;
    }

  if(val & 0x800) /* load */
  { for(i=0;i<8;i++) if(val&(1<<i)) {*c->reg[i] = GETUL(r); r+=4;} }
  else
  { for(i=0;i<8;i++) if(val&(1<<i)) {SETUL(r, *c->reg[i]);  r+=4;} }

  if(n != 8 || !(val & 0x800)) // n not included or stm command
    *c->reg[(val>>8)&7] = r;

  return MAGIC;
}

/* 1  1  0  1  1  1  1  1 --------Value8--------- Software Interrupt */
/* 1  1  0  1 ---Cond---- -------Soffset8-------- Conditional branch */
ULONG cond_branch(CORE *c)
{
  ULONG val = c->opcode;

  if(((val>>8) & 15) == 15) // swi instruction
    return 0x08;
  else
    if(cond_true(c, (val>>8) & 15))
      *c->reg[15] += 2 + ((LONG)(val << 24) >> 23);

  return MAGIC;
}

/* 1  1  1  0  0  -----------Offset11------------ Unconditional branch */
ULONG uncond_branch(CORE *c)
{
  ULONG val = c->opcode;

  return *c->reg[15] += 2 + ((LONG)(val << 21) >> 20);
}

ULONG undefined(CORE *c)
{
  SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: undefined thumb");
  return MAGIC;
}

/* 1  1  1  1  H  -----------Offset11------------ Long branch with link */
ULONG long_branch_link(CORE *c)
{
  int   off;
  ULONG val = c->opcode;

  if(val & 0x800)
  { off = *c->reg[14] | (val & 0x7ff);
    *c->reg[14]  = *c->reg[15] | 1;
    *c->reg[15] += ((LONG)(off << 10) >> 9);
  }
  else
    *c->reg[14]  = val << 11;

  return MAGIC;
}
