#include "cowond2.h"

ULONG GETUL(ULONG des)  { return read_write(des, READU40, 0); }
ULONG GETUS(ULONG des)  { return read_write(des, READU20 + ((des>>1) & 1), 0); }
ULONG GETSS(ULONG des)  { return read_write(des, READS20 + ((des>>1) & 1), 0); }
ULONG GETSB(ULONG des)  { return read_write(des, READS10 + ( des     & 3), 0); }
ULONG GETUB(ULONG des)  { return read_write(des, READU10 + ( des     & 3), 0); }

void SETUL(ULONG des, ULONG val) { read_write(des, WRITE40, val); }
void SETUS(ULONG des, ULONG val) { read_write(des, WRITE20 + ((des>>1) & 1), val); }
void SETUB(ULONG des, ULONG val) { read_write(des, WRITE10 + ( des     & 3), val); }

int allascii(UCHAR *val)
{
  if(((val[0] >= 32 && val[0] < 128) || (val[0] == 9))
  && ((val[1] >= 32 && val[1] < 128) || (val[1] == 9))
  && ((val[2] >= 32 && val[2] < 128) || (val[2] == 9))
  && ((val[3] >= 32 && val[3] < 128) || (val[3] == 9)))
    return 1;
  else
    return 0;
}

ULONG      mapsize;
ULONG      mapind;
char       maps[100000];
MAP_ENTRY  mapentry[2000];

void parse_map_file(char *fname)
{
  FILE   *fil = fopen(fname, "r");
  ULONG  i, j, k, next_line_gives_ptr = 0;
  char   stg[256], txt0[256], txt1[256], txt2[256];
  int    is_icode=0;

  if(fil == 0)  return;  // file does not exist

  while(fgets(stg, 255, fil) != NULL)
  {
#if 0
    /* skip leading tabs and spaces */
    for(i=0; i<256; i++)
      if(stg[i] != 9 && stg[i] != 32)
        break;

    /* assuming .icode is defined directly in front of .idata */
    if(memcmp(stg+i, ".icode", 5) == 0)    is_icode++;
    if(memcmp(stg+i, ".idata", 5) == 0)    is_icode = 0;

    if(is_icode > 1)
    {
      sscanf(stg+i, "0x%x", &mapentry[mapind].fptr);

      for(j=i+10; j<256; j++)
        if(stg[j] == '0' && stg[j+1] == 'x')
          break;

      if(j < 256) continue; /* skip .icode size indication */

      /* search to the start of function name */
      for(i+=10; i<256; i++)
        if(stg[i] != 9 && stg[i] != 32)
          break;

      /* delete trailing data */
      for(k=strlen(stg+i)-1; k>0; k--)
        if(stg[k+i] == 32 || stg[k+i] == 9 || stg[k+i] == 13 || stg[k+i] == 10)
          stg[k+i] = 0;

      strcpy(maps+mapsize, stg+i);
      mapentry[mapind++].fname = maps + mapsize;
      mapsize += strlen(stg+i) + 1;
      continue;
    }

    if(next_line_gives_ptr)
    {
      sscanf(stg+i, "0x%x", &mapentry[mapind++].fptr);
      next_line_gives_ptr = 0;
      continue;
    }

    if(memcmp(stg+i, ".text", 5) == 0)
    {
      /* check if address is stored in the same line */
      for(j=i+6; j<strlen(stg); j++)
        if(stg[j] == '0' && stg[j+1] == 'x')
        { stg[j-1] = 0; break; } // terminate function name

      /* delete trailing data */
      for(k=strlen(stg+i+6)-1; k>0; k--)
        if(stg[k+i+6] == 32 || stg[k+i+6] == 9 || stg[k+i+6] == 13 || stg[k+i+6] == 10)
          stg[k+i+6] = 0;

      strcpy(maps+mapsize, stg+i+6);
      mapentry[mapind].fname = maps + mapsize;
      mapsize += strlen(stg+i+6) + 1;

      if(stg[j] == '0' && stg[j+1] == 'x') /* address in same line */
        sscanf(stg+j, "0x%x", &mapentry[mapind++].fptr);
      else
        next_line_gives_ptr = 1;
    }

    if(mapind >= sizeof(mapentry)/sizeof(mapentry[0]) || mapsize > sizeof(maps))
      SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: overflow in mapbuffer");
#endif
    txt0[0] = txt1[0] = txt2[0] = 0;

    sscanf(stg, "%s %s %s", txt0, txt1, txt2);
 
    if(txt0[0]=='0' && txt0[1]=='x' && txt1[0]!=0 && txt2[0]==0)
    {
      strcpy(maps+mapsize, txt1);
      mapentry[mapind].fname = maps + mapsize;
      mapsize += strlen(txt1) + 1;

      sscanf(txt0+2, "%x", &mapentry[mapind++].fptr);

      if(mapind >= sizeof(mapentry)/sizeof(mapentry[0]) || mapsize > sizeof(maps))
        SendMessage(hWnd, WM_COMMAND, 1, (LPARAM)"ES: overflow in mapbuffer");
    }
  }
  fclose(fil);
}

ULONG      rgs_size;
ULONG      rgs_ind;
char       rgs_dat[20000];
MAP_ENTRY  rgs_entry[512];
/* extract all register definitions */
void parse_head_file(char *fname)
{
  FILE   *fil = fopen(fname, "r");
  char   *p, stg[256];

  if(fil == 0)  return;  // file does not exist

  while(fgets(stg, 255, fil) != NULL)
  {
    if(memcmp(stg, "#define", 7))     continue;
    if((p=strstr(stg, "0x")) == NULL) continue;

    sscanf(stg+8, "%s", rgs_dat+rgs_size);
    rgs_entry[rgs_ind].fname = rgs_dat + rgs_size;
    sscanf(p+2, "%x", &rgs_entry[rgs_ind].fptr);

    if(rgs_entry[rgs_ind].fptr == 0)  continue;
    if(rgs_ind++ >=   510)    break;
    if(rgs_size  >= 20000)    break;

    rgs_size += strlen(rgs_dat+rgs_size) + 1;
  }
  fclose(fil);
}

char *get_symbol(ULONG addr, char *name)
{
  ULONG i;

  sprintf(name, "0x%08x", addr);

  for(i=0; i<rgs_ind; i++)
    if(addr == rgs_entry[i].fptr)
    { strcpy(name, rgs_entry[i].fname); break; }

  return name;
}

char *format_stg(char *stg, ULONG stop)
{
  ULONG i;

  if(memcmp(stg, "0x", 2) == 0)
    strcpy(stg, "                ");

  /* extend string to always 15 characters */
  for(i=strlen(stg); i<stop; i++)
    stg[i] = 32;
  stg[stop] = 0;

  return stg;
}

void sadjust(char *stg, ULONG pos, ULONG ret)
{
  ULONG i, p=0;

  for(i=0; stg[i]; i++)
  {
    if(stg[i] == 9) p = (p + 4) & ~3; // tab found
    else            p++;
  }

  while((p & ~3) < pos-5)
  { stg[i++] = 9;
    stg[ i ] = 0;
    p += 4;
  }

  sprintf(stg, "%s	;	0x%08x", stg, ret);
}

void init_asm_buffer(ASM_BUFFER *asmbuf, char *fname)
{
  asmbuf->ip      = 255;
  asmbuf->ipstart = 255;
  asmbuf->fil     = fopen(fname, "w");
  memset(asmbuf->addrss, 0xff, sizeof(asmbuf->addrss));
}

void check_for_loop(ASM_BUFFER* asmbuf, ULONG pc, ULONG ind, char* stg) {
    ULONG j;

    if (!asmbuf || !asmbuf->fil || !stg) return;   /* NULL 保护 */

    if (asmbuf->cnt) {
        if (pc == asmbuf->addrss[(asmbuf->ip - asmbuf->ichk) & 255]) {
            if (((asmbuf->ips + asmbuf->ichk) & 255) == asmbuf->ip) {
                asmbuf->cnt++;
                asmbuf->ip = (asmbuf->ips - 1) & 255;
            }
        }
        else {
            if (asmbuf->cnt > 1) {
                for (j = asmbuf->ipstart; (asmbuf->ips - asmbuf->ichk - j) & 255; j++) {
                    if (asmbuf->addrss[j & 255] != 0xffffffff) {
                        fprintf(asmbuf->fil, "%s", asmbuf->strings[j & 255]);
                        asmbuf->addrss[j & 255] = 0xffffffff;
                    }
                }
                fprintf(asmbuf->fil, "\n-------------start--------------------");
                for (; (asmbuf->ips + 1 - j) & 255; j++) {
                    fprintf(asmbuf->fil, "%s", asmbuf->strings[j & 255]);
                    asmbuf->addrss[j & 255] = 0xffffffff;
                }
                fprintf(asmbuf->fil, "\n----------loop x %d (0x%x) ----------",
                    asmbuf->cnt, asmbuf->cnt);
                asmbuf->ipstart = (j - 1) & 255;
            }
            asmbuf->cnt = 0;
        }
    }

    if (asmbuf->cnt == 0) {
        for (asmbuf->ichk = 0; asmbuf->ichk < 128; asmbuf->ichk++) {
            if (pc == asmbuf->addrss[asmbuf->ip - asmbuf->ichk & 255]) {
                asmbuf->cnt = 1;
                asmbuf->ips = asmbuf->ip;
                break;
            }
        }
    }

    asmbuf->ip = (asmbuf->ip + 1) & 255;
    if (asmbuf->ip == asmbuf->ipstart) {
        if (asmbuf->addrss[asmbuf->ip] != 0xffffffff)
            fprintf(asmbuf->fil, "%s", asmbuf->strings[asmbuf->ip]);
        asmbuf->ipstart = (asmbuf->ipstart + 1) & 255;
    }

    stg[110] = 0;
    asmbuf->addrss[asmbuf->ip] = pc;
    snprintf(asmbuf->strings[asmbuf->ip], 256, "\n%07x %s", ind, stg);
    // ^^^^^^^^ 用 snprintf 限制写入长度，防止溢出
}

void flush_loop_end(ASM_BUFFER *asmbuf, CORE *c, ULONG ind)
{
  int i;

  strcpy(c->stg, "\n<<<<<<<<<Terminated>>>>>>>>>>>>");
  check_for_loop(asmbuf, 0, ind, c->stg);

  for(i=255; i>=0; i--)
    if(asmbuf->addrss[asmbuf->ip - i & 255] != 0xffffffff)
      fprintf(asmbuf->fil, "%s", asmbuf->strings[asmbuf->ip - i & 255]);

  fclose(asmbuf->fil);
}
