#include "cowond2.h"

ULONG base0; /* exported for disassembler usage */

ULONG cond_true(CORE *c, ULONG val)
{
  /*  0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15 */
  /* eq, ne, cs, cc, mi, pl, vs, vc, hi, ls, ge, lt, gt, le, al, nv */
  switch(val)
  {
  case  0: return ctrue =  c->cpsr->Z;
  case  1: return ctrue = !c->cpsr->Z;
  case  2: return ctrue =  c->cpsr->C;
  case  3: return ctrue = !c->cpsr->C;
  case  4: return ctrue =  c->cpsr->N;
  case  5: return ctrue = !c->cpsr->N;
  case  6: return ctrue =  c->cpsr->V;
  case  7: return ctrue = !c->cpsr->V;
  case  8: return ctrue =  c->cpsr->C && !c->cpsr->Z;
  case  9: return ctrue = !c->cpsr->C ||  c->cpsr->Z;
  case 10: return ctrue =  c->cpsr->N ==  c->cpsr->V;
  case 11: return ctrue =  c->cpsr->N !=  c->cpsr->V;
  case 12: return ctrue = !c->cpsr->Z && (c->cpsr->N == c->cpsr->V);
  case 13: return ctrue =  c->cpsr->Z || (c->cpsr->N != c->cpsr->V);
  case 14: return ctrue =  1;
  default: SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: INSTR0 undef"); return ctrue = 0;
  }
}

ULONG get_offset(CORE *c, ULONG val, ULONG operand2, ULONG set_cond)
{
  ULONG           shft, reg0, reg1;
  unsigned S64 off;

  reg0 = *c->reg[(val>>0)&15] + (((val>>0) & 15) == 15 ? 4 : 0);
  reg1 = *c->reg[(val>>8)&15] + (((val>>8) & 15) == 15 ? 4 : 0);

  if(((val>>25) & 1) ^ operand2) // operand2: b25=0 => reg offset, offset: b25=1 => reg offset
  {
    if(val & 16) // reg defined shift
    { shft = reg1 & 0xff;
      if(shft == 0)
        return reg0;
    }
    else
    { shft = (val>>7) & 31;
      if(shft == 0)
        if(((val>>5) & 3) == 0) // lsl #0
          return reg0;
        else
          if(((val>>5) & 3) != 3) // not: 'ror #0' which is rrx
            shft = 32;
    }

    if(shft == 32)
      switch((val>>5) & 3) // 0=lsl, 1=lsr, 2=asr, 3=ror
      {
      case 0: off = (reg0 & 0x00000001 ? 0x100000000L : 0); break;
      case 1: off = (reg0 & 0x80000000 ? 0x100000000L : 0); break;
      case 2: off = (reg0 & 0x80000000 ? 0x1ffffffffL : 0); break;
      case 3: off = (reg0 & 0x80000000 ? 0x100000000L : 0) | reg0; break;
      }
    else if(shft > 32)
      switch((val>>5) & 3) // 0=lsl, 1=lsr, 2=asr, 3=ror
      {
      case 0: off = 0; break;
      case 1: off = 0; break;
      case 2: off = reg0 & 0x80000000 ? 0x1ffffffffL : 0; break;
      case 3: if((shft&31) == 0) off = ((U64)reg0 << 32) + (reg0 >> 1) + (c->cpsr->C << 31);
              else               off = ((U64)reg0 << (32 - (shft&31))) + (reg0 >> (shft&31));
      }
    else
      switch((val>>5) & 3) // 0=lsl, 1=lsr, 2=asr, 3=ror
      {
      case 0: off =   (U64)reg0 << shft; break;
      case 1: off = (((U64)reg0 << (33-shft)) & 0x100000000L) + (reg0 >> shft); break;
      case 2: off = (((U64)reg0 << (33-shft)) & 0x100000000L) + (((long)reg0 >> shft) & 0xffffffff); break;
      case 3: if((shft&31) == 0) off = ((U64)reg0 << 32) + (reg0 >> 1) + (c->cpsr->C << 31);
              else               off = ((U64)reg0 << (32 - (shft&31))) + (reg0 >> (shft&31));
      }

    if(set_cond) // set conditions
      c->cpsr->C = off & 0x100000000L ? 1 : 0;
  }
  else
  { if(operand2)
      off = ((unsigned S64)(val&0xff) << (32 - 2 * ((val >> 8) & 15))) + ((val&0xff) >> 2 * ((val >> 8) & 15));
    else
      off = val & 0xfff;
  }

  return (ULONG)off;
}

/*
31-28 27 26 25 24  23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
Cond  0  0  I  ---Opcode--- S |----Rn----- ----Rd----- --------Operand 2-------- Data Processing /PSR Transfer
Cond  0  0  0  0 | 0  0  A  S |----Rd----- ----Rn----- ---Rs---- 1 0 0 1 --Rm--- Multiply
Cond  0  0  0  0 | 1  U  A  S |---RdHi---- ---RdLo---- ---Rn---- 1 0 0 1 --Rm--- Multiply Long
Cond  0  0  0  1 | 0  B  0  0 |----Rn----- ----Rd----- 0  0  0 0 1 0 0 1 --Rm--- Single Data Swap
Cond  0  0  0  1 | 0  0  1  0 |1  1  1  1  1  1  1  1  1  1  1 1 0 0 0 1 --Rn--- Branch and Exchange
Cond  0  0  0  P | U  0  W  L |----Rn----- ----Rd----- 0  0  0 0 1 S H 1 --Rm--- Halfword Data Transfer: register offset
Cond  0  0  0  P | U  1  W  L |----Rn----- ----Rd----- --Offset- 1 S H 1 -Offset Halfword Data Transfer: immediate offset
Cond  0  1  I  P | U  B  W  L |----Rn----- ----Rd----- --------Offset----------- Single Data Transfer
Cond  0  1  1  1 | x  x  x  x |x  x  x  x  x  x  x  x  x  x  x x x x x 1 x x x x Undefined
Cond  1  0  0  P | U  S  W  L |----Rn----- -----------Register List------------- Block Data Transfer
Cond  1  0  1  L | -------------------------Offset------------------------------ Branch
Cond  1  1  0  P | U  N  W  L |----Rn----- ----CRd---- ---CP#--- -----Offset---- Coprocessor Data Transfer
Cond  1  1  1  0 | --CP Opc---|----CRn---- ----CRd---- ---CP#--- -CP-- 0 --CRm-- Coprocessor Data Operation
Cond  1  1  1  0 | CP Opc   L |----CRn---- ----Rd----- ---CP#--- -CP-- 1 --CRm-- Coprocessor Register Transfer
Cond  1  1  1  1 | x  x  x  x |x  x  x  x  x  x  x  x  x  x  x x x x x x x x x x Software Interrupt

EQ 0 Z set equal
NE 1 Z clear not equal
CS 2 C set unsigned higher or same
CC 3 C clear unsigned lower
MI 4 N set negative
PL 5 N clear positive or zero
VS 6 V set overflow
VC 7 V clear no overflow
HI 8 C set and Z clear unsigned higher
LS 9 C clear or Z set unsigned lower or same
GE A N equals V greater or equal
LT B N not equal to V less than
GT C Z clear AND (N equals V) greater than
LE D Z set OR (N not equal to V) less than or equal
AL E (ignored) always

AND 0 operand1 AND operand2
EOR 1 operand1 EOR operand2
SUB 2 operand1 - operand2
RSB 3 operand2 - operand1
ADD 4 operand1 + operand2
ADC 5 operand1 + operand2 + carry
SBC 6 operand1 - operand2 + carry - 1
RSC 7 operand2 - operand1 + carry - 1
TST 8 AND, but result is not written
TEQ 9 as EOR, but result is not written
CMP A as SUB, but result is not written
CMN B as ADD, but result is not written
ORR C operand1 OR operand2
MOV D operand2 (operand1 is ignored)
BIC E operand1 AND NOT operand2 (Bit clear)
MVN F NOT operand2 (operand1 is ignored)
NZCV
*/

/*
Cond  0  0  0  0 | 0  0  A  S |----Rd----- ----Rn----- ---Rs---- 1 0 0 1 --Rm--- Multiply
Cond  0  0  0  0 | 1  U  A  S |---RdHi---- ---RdLo---- ---Rn---- 1 0 0 1 --Rm--- Multiply Long */
ULONG multiply(CORE *c, ULONG val)
{
	S64    res;
  ULONG  rm, rs, rn, rd;

  rm = *c->reg[(val>> 0)&15] + (((val>> 0) & 15) == 15 ? 4 : 0);
  rs = *c->reg[(val>> 8)&15] + (((val>> 8) & 15) == 15 ? 4 : 0);
  rn = *c->reg[(val>>12)&15] + (((val>>12) & 15) == 15 ? 4 : 0);
  rd = *c->reg[(val>>16)&15] + (((val>>16) & 15) == 15 ? 4 : 0);

  if((val&0xc00000) == 0) // simple mul
  {
    if(val & 0x200000)
      res = (S64)rm * rs + rn;
    else
      res = (S64)rm * rs;

    if(val & 0x100000) // set condition flags
    {
      c->cpsr->Z = (res & 0xffffffff) == 0 ? 1 : 0;
      c->cpsr->N = (res & 0x80000000)      ? 1 : 0;
      c->cpsr->C = (res & 0x100000000L)    ? 1 : 0; // not sure
    }

    *c->reg[(val>>16)&15] = (ULONG)res; // rd
  }
  else
  {
    if(val & 0x200000) // accumulate
      if(val & 0x400000) // signed
        res = (S64)(long)rm * (S64)(long)rs + (((S64)rd << 32) | rn);
      else
        res = (U64)rm * (U64)rs + (((U64)rd << 32) | rn);
    else
      if(val & 0x400000) // signed
        res = (S64)(long)rm * (S64)(long)rs;
      else
        res = (U64)rm * (U64)rs;

    if(val & 0x100000) // set condition flags
    {
      c->cpsr->Z = (res == 0)                  ? 1 : 0;
      c->cpsr->N = (res & 0x8000000000000000L) ? 1 : 0;
      c->cpsr->C = (res & 0x100000000L)        ? 1 : 0; // not sure
      c->cpsr->V = (res < 0xffffffff80000000L) || (res >= 0x7fffffff) ? 1 : 0; // not sure
    }
    
    *c->reg[(val>>16)&15] = (ULONG)(res >> 32);
    *c->reg[(val>>12)&15] = (ULONG)(res >>  0);
  }
  return (ULONG)res;
}

/*
Cond  0  0  0  P | U  0  W  L |----Rn----- ----Rd----- 0  0  0 0 1 S H 1 --Rm--- Halfword Data Transfer: register offset
Cond  0  0  0  P | U  1  W  L |----Rn----- ----Rd----- --Offset- 1 S H 1 -Offset Halfword Data Transfer: immediate offset */
ULONG halfword(CORE *c, ULONG val)
{
   LONG off   = ((val>>4) & 0xf0) + (val & 0x0f);
  ULONG breg  = (val>>16) & 15;
  ULONG base1 = base0 = *c->reg[breg]; // address after r/w
  ULONG res;

  if(breg == 15) // pc needs additional offset
    base0 += 4;

  off    =  val & 0x00400000 ? off : *c->reg[val&15]; // immediate offset : reg offset
  off    =  val & 0x00800000 ? off : -off;            // up  : down
  base0 +=  val & 0x01000000 ? off :    0;            // pre : post indexed
  base1 += (val & 0x00200000) || ((val & 0x01000000) == 0) ? off : 0; // write back if 'write back' bit or 'post indx'

  switch((val>>5) & 3) // SWP, HW, SB, SH
  {
  case 0: SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: SWP");              break;
  case 1:
  case 3: if(base0 & 1)
            SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: alignment0");
          break;
  }

  if(val & 0x100000) // load
  {
    switch((val>>5) & 3) // SWP, HW, SB, SH
    {
    case 1: *c->reg[(val>>12)&15] = res = GETUS(base0); break;
    case 2: *c->reg[(val>>12)&15] = res = GETSB(base0); break;
    case 3: *c->reg[(val>>12)&15] = res = GETSS(base0); break;
    }
  }
  else
  { if(breg == 15) base0 -= 4; // TODO: special for 'str pc,...'

    switch((val>>5) & 3) // SWP, HW, SB, SH
    {
    case 1: SETUS(base0, res = *c->reg[(val>>12)&15]); break;
    case 2: SETUB(base0, res = *c->reg[(val>>12)&15]); break;
    case 3: SETUS(base0, res = *c->reg[(val>>12)&15]); break;
    }
  }

  if(breg != ((val>>12)&15) || !(val & 0x100000)) // no write back on 'ldr rx, [rx...'
    *c->reg[breg] = base1;

  return res;
}

/* Cond  0  0  0  1 | 0  0  1  0 |1  1  1  1  1  1  1  1  1  1  1 1 0 0 0 1 --Rn--- Branch & Exchange */
ULONG branchexchg(CORE *c, ULONG val, int blx_instr)
{
  if(blx_instr)
    *c->reg[14] = *c->reg[15];

  *c->reg[15] = val & 0xfffffffe; // ignore bit0
  c->cpsr->T  = val & 1 ? 1 : 0;  // thumb mode <=> arm mode

  return MAGIC;
}

/* Cond  1  0  1  L | -------------------------Offset------------------------------ Branch */
ULONG branch(CORE *c)
{
  ULONG val = c->opcode;

  if(val & 0x1000000) // branch with link
    *c->reg[14] = *c->reg[15];

  *c->reg[15] += ((long)(val << 8) >> 6) + 4;

  return MAGIC;
}

/*
Cond  0  0  I  ---Opcode--- S |----Rn----- ----Rd----- --------Operand 2-------- Data Processing /PSR Transfer
Cond  0  0  0  1 | 0  0  1  0 |1  1  1  1  1  1  1  1  1  1  1 1 0 0 0 1 --Rn--- Branch and Exchange
Cond  0  0  0  0 | 0  0  A  S |----Rd----- ----Rn----- ---Rs---- 1 0 0 1 --Rm--- Multiply
Cond  0  0  0  0 | 1  U  A  S |---RdHi---- ---RdLo---- ---Rn---- 1 0 0 1 --Rm--- Multiply Long
Cond  0  0  0  1 | 0  0  1  0 |1  1  1  1  1  1  1  1  1  1  1 1 0 0 0 1 --Rn--- Branch and Exchange
Cond  0  0  0  P | U  0  W  L |----Rn----- ----Rd----- 0  0  0 0 1 S H 1 --Rm--- Halfword Data Transfer: register offset
Cond  0  0  0  P | U  1  W  L |----Rn----- ----Rd----- --Offset- 1 S H 1 -Offset Halfword Data Transfer: immediate offset */
ULONG opcode(CORE *c)
{
  ULONG        val = c->opcode;
  ULONG        ua, ub, des, off, mask;
  S64          ir, ia, ib;
  unsigned S64 ur;

  if((val & 0x0ffffff0) == 0x012fff10)
  { return branchexchg(c, *c->reg[val&15], 0); }
  else if((val & 0x0f0000f0) == 0x00000090)
  { return multiply(c, val); }
  else if(((val & 0x0f000000) <= 0x01000000) && ((val & 0x90) == 0x90) && ((val & 0xf0) > 0x90) && ((val & 0x01200000) != 0x00200000))
  { return halfword(c, val); }
  else if((((val>>21) & 15) >= 8) && (((val>>21) & 15) < 12) && !(val & 0x100000))
  { // TEQ,TST,CMP,CMN without S-bit => msr, mrs instructions
    if((val & 0x0ffffff0) == 0x012fff30) /* blx */
    {
      return branchexchg(c, *c->reg[val&15], 1);
    }
    else
    {
      off  = get_offset(c, val, 1, 0);
      mask = 0;

      /* TODO: verify that all fields can be set in user mode */
      if(val & 0x10000)  mask |= c->cpsr->M == 0x10 ? 0 : 0x000000ff; // control bits   ?????
      if(val & 0x20000)  mask |= c->cpsr->M == 0x10 ? 0 : 0x0000ff00; // extension bits ?????
      if(val & 0x40000)  mask |= c->cpsr->M == 0x10 ? 0 : 0x0fff0000; // state bits     ?????
      if(val & 0x80000)  mask |=                          0xf0000000; // flags bits

      if((val & 0x0fbf0fff) == 0x010f0000)
      { if(val & 0x400000) *c->reg[(val>>12)&15] = ua = c->cpsr->M == 0x10 ? *(ULONG*)c->cpsr : *(ULONG*)c->spsr;
        else               *c->reg[(val>>12)&15] = ua = *(ULONG*)c->cpsr;
      }
      else if((val & 0x0fb0fff0) == 0x0120f000) // register operand
      { if(val & 0x400000) { if(c->spsr) *(ULONG*)c->spsr = ua = (*(ULONG*)c->spsr & ~mask) | (*c->reg[val&15] & mask); }
        else               {             *(ULONG*)c->cpsr = ua = (*(ULONG*)c->cpsr & ~mask) | (*c->reg[val&15] & mask); }
      }
      else if((val & 0x0fb0f000) == 0x0320f000) // immediate operand
      { if(val & 0x400000) *(ULONG*)c->spsr = ua = (*(ULONG*)c->spsr & ~mask) | (off & mask);
        else               *(ULONG*)c->cpsr = ua = (*(ULONG*)c->cpsr & ~mask) | (off & mask);
      }
      else if((val & 0x0ff0f0f0) == 0x01600080)
      { *c->reg[(val>>16)&15] = ua = (*c->reg[val&15] & 0xffff) * (*c->reg[(val>>8)&15] & 0xffff); }
      else if((val & 0x0ff0f0f0) == 0x016000a0)
      { *c->reg[(val>>16)&15] = ua = (*c->reg[val&15]  >>   16) * (*c->reg[(val>>8)&15] & 0xffff); }
      else if((val & 0x0ff0f0f0) == 0x016000c0)
      { *c->reg[(val>>16)&15] = ua = (*c->reg[val&15] & 0xffff) * (*c->reg[(val>>8)&15]  >>   16); }
      else if((val & 0x0ff0f0f0) == 0x016000e0)
      { *c->reg[(val>>16)&15] = ua = (*c->reg[val&15]  >>   16) * (*c->reg[(val>>8)&15]  >>   16); }
      else if((val & 0x0ff000f0) == 0x01000080)
      { *c->reg[(val>>16)&15] = ua = *c->reg[(val>>12)&15] + (*c->reg[val&15] & 0xffff) * (*c->reg[(val>>8)&15] & 0xffff); }
      else if((val & 0x0ff000f0) == 0x010000a0)
      { *c->reg[(val>>16)&15] = ua = *c->reg[(val>>12)&15] + (*c->reg[val&15]  >>   16) * (*c->reg[(val>>8)&15] & 0xffff); }
      else if((val & 0x0ff000f0) == 0x010000c0)
      { *c->reg[(val>>16)&15] = ua = *c->reg[(val>>12)&15] + (*c->reg[val&15] & 0xffff) * (*c->reg[(val>>8)&15]  >>   16); }
      else if((val & 0x0ff000f0) == 0x010000e0)
      { *c->reg[(val>>16)&15] = ua = *c->reg[(val>>12)&15] + (*c->reg[val&15]  >>   16) * (*c->reg[(val>>8)&15]  >>   16); }
      else if((val & 0x0fff0ff0) == 0x016f0f10) /* clz instruction (count leading zeros) */
      { for(ua=0; ua<32; ua++)
          if(*c->reg[val&15] & (1<<31-ua))
            break;
        *c->reg[(val>>12)&15] = ua;
      }

      return ua;
    }
  }

  des = (val>>12) & 15;
  ua  = *c->reg[(val>>16) & 15] + (((val>>16) & 15) == 15 ? 4 : 0);
  ub  = get_offset(c, val, 1, val & 0x100000);
  ia  = (LONG)ua;
  ib  = (LONG)ub;

  // TODO: correct flagging?
  switch((val>>21) & 15)
  {
  case  0: ur = *c->reg[des] =                        ua & ub;                   break;
  case  1: ur = *c->reg[des] =                        ua ^ ub;                   break;
  case  2: *c->reg[des] = (ULONG)(ur = 0x100000000L + ua - ub);                  ir = (S64)ia - ib; break;
  case  3: *c->reg[des] = (ULONG)(ur = 0x100000000L + ub - ua);                  ir = (S64)ib - ia; break;
  case  4: *c->reg[des] = (ULONG)(ur =           (U64)ua + ub);                  ir = (S64)ia + ib; break;
  case  5: *c->reg[des] = (ULONG)(ur =           (U64)ua + ub + c->cpsr->C);     ir = (S64)ia + ib + c->cpsr->C;     break;
  case  6: *c->reg[des] = (ULONG)(ur = 0x100000000L + ua - ub + c->cpsr->C - 1); ir = (S64)ia - ib + c->cpsr->C - 1; break;
  case  7: *c->reg[des] = (ULONG)(ur = 0x100000000L + ub - ua + c->cpsr->C - 1); ir = (S64)ib - ia + c->cpsr->C - 1; break;
  case  8:                        ur =                ua & ub;                   break;
  case  9:                        ur =                ua ^ ub;                   break;
  case 10:                        ur = 0x100000000L + ua - ub;                   ir = (S64)ia - ib; break;
  case 11:                        ur =           (U64)ua + ub;                   ir = (S64)ia + ib; break;
  case 12: ur = *c->reg[des] =                        ua | ub;                   break;
  case 13: ur = *c->reg[des] =                        ub;                        break;
  case 14: ur = *c->reg[des] =                        ua & ~ub;                  break;
  case 15: ur = *c->reg[des] =                       ~ub;                        break;
  }

  if(val & 0x100000) // set conditional flags
  {
    c->cpsr->Z = ur & 0xffffffff ? 0 : 1;
    c->cpsr->N = ur & 0x80000000 ? 1 : 0;

    switch((val>>21) & 15)
    {
    case 2: case 3: case 4: case 5: case 6: case 7: case 10: case 11:
      c->cpsr->V = ((ir>>31) == 0) || ((ir>>31) == -1) ? 0 : 1;
      c->cpsr->C = (ur & 0x100000000) ? 1 : 0;
    }
  }
  return (ULONG)ur;
}

/* Cond  0  1  I  P | U  B  W  L |----Rn----- ----Rd----- --------Offset----------- Single Data Transfer */
ULONG single_transfer(CORE *c)
{
   LONG off;
  ULONG res;
  ULONG val   = c->opcode;
  ULONG breg  = (val>>16) & 15;
  ULONG base1 = base0 = *c->reg[breg]; // address after r/w

  if(breg == 15) // pc needs additional offset
    base0 += 4;

  if(((val & 0x0e000000) == 0x06000000) && (val & 16))
    SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: INSTR1 undef");

  if((val & 0x2000000) && (val & 16)) // reg offset && reg defined shift
    SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: INSTR2 undef");

  off    = get_offset(c, val, 0, 0);
  off    =  val & 0x00800000 ? off : -off; // up : down
  base0 +=  val & 0x01000000 ? off :    0; // pre : post indexed
  base1 += (val & 0x00200000) || ((val & 0x01000000) == 0) ? off : 0; // write back if 'write back' bit or 'post indx'

  if((val & 0x400000) == 0) // not byte operation
    if(base0 & 3)
      SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: alignment1");

  if(val & 0x100000) // load
    if(val & 0x400000) *c->reg[(val>>12)&15] = res = GETUB(base0); // byte
    else               *c->reg[(val>>12)&15] = res = GETUL(base0); // long
  else
  { if(breg == 15) base0 -= 4; // TODO: special for 'str pc,...'
    if(val & 0x400000) SETUB(base0, res = *c->reg[(val>>12)&15]); // byte
    else               SETUL(base0, res = *c->reg[(val>>12)&15]); // long
  }

  if(breg != ((val>>12)&15) || !(val & 0x100000)) // no write back on 'ldr rx, [rx...'
    *c->reg[breg] = base1;

  return res;
}

/* Cond  1  0  0  P | U  S  W  L |----Rn----- -----------Register List------------- Block Transfer */
ULONG multip_transfer(CORE *c)
{
  ULONG val = c->opcode;
  int   i, cnt=0, wrt=0, n=(val>>16)&15;
  ULONG r = *c->reg[n];
  ULONG use_user_regs = 0;

  if(val & 0x400000) // load psr or force user mode
    if(val & 0x8000) // pc is included in the transfer list
      if(val & 0x100000) *c->cpsr = c->spsr ? *c->spsr : *c->cpsr; // in case of load  (ldm command)
      else               use_user_regs = 1; // in case of store (stm command)
    else                 use_user_regs = 1; // both load & store commands

  if(val & 0x200000)
    for(i=0;i<16;i++)
      if(val & (1<<i))
      { wrt += (val & 0x800000) ? 4 : -4;
        if(cnt != 0 && i == n && !(val & 0x100000)) // n included, but not first in stm command
          SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: danger stm");
        if(i == n) // n is included
          n = 16;
        cnt++;
      }

  if(val & 0x800000) // increment
    if(val & 0x100000) // load
      if(use_user_regs)
        if(val & 0x1000000) {for(i=0;i<16;i++) if(val & (1<<i)) {  c->rg[i] = GETUL(r+=4);} }    // pre offset
        else                {for(i=0;i<16;i++) if(val & (1<<i)) {  c->rg[i] = GETUL(r); r+=4;} } // post offset
      else
        if(val & 0x1000000) {for(i=0;i<16;i++) if(val & (1<<i)) {*c->reg[i] = GETUL(r+=4);} }    // pre offset
        else                {for(i=0;i<16;i++) if(val & (1<<i)) {*c->reg[i] = GETUL(r); r+=4;} } // post offset
    else
      if(use_user_regs)
        if(val & 0x1000000) {for(i=0;i<16;i++) if(val & (1<<i)) {SETUL(r+=4,  c->rg[i]);} }     // pre offset
        else                {for(i=0;i<16;i++) if(val & (1<<i)) {SETUL(r,  c->rg[i]); r+=4; } } // post offset
      else
        if(val & 0x1000000) {for(i=0;i<16;i++) if(val & (1<<i)) {SETUL(r+=4,*c->reg[i]);} }     // pre offset
        else                {for(i=0;i<16;i++) if(val & (1<<i)) {SETUL(r,*c->reg[i]); r+=4; } } // post offset
  else
    if(val & 0x100000) // load
      if(use_user_regs)
        if(val & 0x1000000) {for(i=16;i--;) if(val & (1<<i)) {  c->rg[i] = GETUL(r-=4);} }    // pre offset
        else                {for(i=16;i--;) if(val & (1<<i)) {  c->rg[i] = GETUL(r); r-=4;} } // post offset
      else
        if(val & 0x1000000) {for(i=16;i--;) if(val & (1<<i)) {*c->reg[i] = GETUL(r-=4);} }    // pre offset
        else                {for(i=16;i--;) if(val & (1<<i)) {*c->reg[i] = GETUL(r); r-=4;} } // post offset
    else
      if(use_user_regs)
        if(val & 0x1000000) {for(i=16;i--;) if(val & (1<<i)) {SETUL(r-=4,  c->rg[i]);} }    // pre offset
        else                {for(i=16;i--;) if(val & (1<<i)) {SETUL(r,  c->rg[i]); r-=4;} } // pre offset
      else
        if(val & 0x1000000) {for(i=16;i--;) if(val & (1<<i)) {SETUL(r-=4,*c->reg[i]);} }    // pre offset
        else                {for(i=16;i--;) if(val & (1<<i)) {SETUL(r,*c->reg[i]); r-=4;} } // pre offset

  if(n != 16 || ((val & 0x100000) == 0)) // n not included or stm command
    *c->reg[(val>>16)&15] += wrt;

  return MAGIC;
}
