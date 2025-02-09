/* Pawn disassembler  - crude, but (perhaps) useful
 *
 *  Copyright (c) ITB CompuPhase, 2007
 *
 *  This software is provided "as-is", without any express or implied warranty.
 *  In no event will the authors be held liable for any damages arising from
 *  the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1.  The origin of this software must not be misrepresented; you must not
 *      claim that you wrote the original software. If you use this software in
 *      a product, an acknowledgment in the product documentation would be
 *      appreciated but is not required.
 *  2.  Altered source versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software.
 *  3.  This notice may not be removed or altered from any source distribution.
 *
 *  Version: $Id$
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sc.h"
#include "../amx/amx.h"
#include "../amx/amxdbg.h"

static FILE *fpamx;
static AMX_HEADER amxhdr;
static int dbgloaded;
static AMX_DBG amxdbg;

typedef cell (*OPCODE_PROC)(FILE *ftxt,const cell *params,cell opcode,cell cip);

#define MAX_OPCODE_LIST (sizeof(opcodelist) / sizeof(opcodelist[0]))

cell parm0(FILE *ftxt,const cell *params,cell opcode,cell cip);
cell parm1(FILE *ftxt,const cell *params,cell opcode,cell cip);
cell parm2(FILE *ftxt,const cell *params,cell opcode,cell cip);
cell parm3(FILE *ftxt,const cell *params,cell opcode,cell cip);
cell parm4(FILE *ftxt,const cell *params,cell opcode,cell cip);
cell parm5(FILE *ftxt,const cell *params,cell opcode,cell cip);
cell do_proc(FILE *ftxt,const cell *params,cell opcode,cell cip);
cell do_call(FILE *ftxt,const cell *params,cell opcode,cell cip);
cell do_jump(FILE *ftxt,const cell *params,cell opcode,cell cip);
cell do_sysreq(FILE *ftxt,const cell *params,cell opcode,cell cip);
cell do_casetbl(FILE *ftxt,const cell *params,cell opcode,cell cip);


typedef struct {
  char *name;
  OPCODE_PROC func;
} OPCODE;

static OPCODE opcodelist[] = {
  { /*  0*/ NULL,         NULL },
  { /*  1*/ "load.pri",   parm1 },
  { /*  2*/ "load.alt",   parm1 },
  { /*  3*/ "load.s.pri", parm1 },
  { /*  4*/ "load.s.alt", parm1 },
  { /*  5*/ "lref.pri",   parm1 },
  { /*  6*/ "lref.alt",   parm1 },
  { /*  7*/ "lref.s.pri", parm1 },
  { /*  8*/ "lref.s.alt", parm1 },
  { /*  9*/ "load.i",     parm0 },
  { /* 10*/ "lodb.i",     parm1 },
  { /* 11*/ "const.pri",  parm1 },
  { /* 12*/ "const.alt",  parm1 },
  { /* 13*/ "addr.pri",   parm1 },
  { /* 14*/ "addr.alt",   parm1 },
  { /* 15*/ "stor.pri",   parm1 },
  { /* 16*/ "stor.alt",   parm1 },
  { /* 17*/ "stor.s.pri", parm1 },
  { /* 18*/ "stor.s.alt", parm1 },
  { /* 19*/ "sref.pri",   parm1 },
  { /* 20*/ "sref.alt",   parm1 },
  { /* 21*/ "sref.s.pri", parm1 },
  { /* 22*/ "sref.s.alt", parm1 },
  { /* 23*/ "stor.i",     parm0 },
  { /* 24*/ "strb.i",     parm1 },
  { /* 25*/ "lidx",       parm0 },
  { /* 26*/ "lidx.b",     parm1 },
  { /* 27*/ "idxaddr",    parm0 },
  { /* 28*/ "idxaddr.b",  parm1 },
  { /* 29*/ "align.pri",  parm1 },
  { /* 30*/ "align.alt",  parm1 },
  { /* 31*/ "lctrl",      parm1 },
  { /* 32*/ "sctrl",      parm1 },
  { /* 33*/ "move.pri",   parm0 },
  { /* 34*/ "move.alt",   parm0 },
  { /* 35*/ "xchg",       parm0 },
  { /* 36*/ "push.pri",   parm0 },
  { /* 37*/ "push.alt",   parm0 },
  { /* 38*/ "push.r",     parm1 },  /* obsolete (never generated) */
  { /* 39*/ "push.c",     parm1 },
  { /* 40*/ "push",       parm1 },
  { /* 41*/ "push.s",     parm1 },
  { /* 42*/ "pop.pri",    parm0 },
  { /* 43*/ "pop.alt",    parm0 },
  { /* 44*/ "stack",      parm1 },
  { /* 45*/ "heap",       parm1 },
  { /* 46*/ "proc",       do_proc },
  { /* 47*/ "ret",        parm0 },
  { /* 48*/ "retn",       parm0 },
  { /* 49*/ "call",       do_call },
  { /* 50*/ "call.pri",   parm0 },
  { /* 51*/ "jump",       do_jump },
  { /* 52*/ "jrel",       parm1 },  /* same as jump, since version 10 */
  { /* 53*/ "jzer",       do_jump },
  { /* 54*/ "jnz",        do_jump },
  { /* 55*/ "jeq",        do_jump },
  { /* 56*/ "jneq",       do_jump },
  { /* 57*/ "jless",      do_jump },
  { /* 58*/ "jleq",       do_jump },
  { /* 59*/ "jgrtr",      do_jump },
  { /* 60*/ "jgeq",       do_jump },
  { /* 61*/ "jsless",     do_jump },
  { /* 62*/ "jsleq",      do_jump },
  { /* 63*/ "jsgrtr",     do_jump },
  { /* 64*/ "jsgeq",      do_jump },
  { /* 65*/ "shl",        parm0 },
  { /* 66*/ "shr",        parm0 },
  { /* 67*/ "sshr",       parm0 },
  { /* 68*/ "shl.c.pri",  parm1 },
  { /* 69*/ "shl.c.alt",  parm1 },
  { /* 70*/ "shr.c.pri",  parm1 },
  { /* 71*/ "shr.c.alt",  parm1 },
  { /* 72*/ "smul",       parm0 },
  { /* 73*/ "sdiv",       parm0 },
  { /* 74*/ "sdiv.alt",   parm0 },
  { /* 75*/ "umul",       parm0 },
  { /* 76*/ "udiv",       parm0 },
  { /* 77*/ "udiv.alt",   parm0 },
  { /* 78*/ "add",        parm0 },
  { /* 79*/ "sub",        parm0 },
  { /* 80*/ "sub.alt",    parm0 },
  { /* 81*/ "and",        parm0 },
  { /* 82*/ "or",         parm0 },
  { /* 83*/ "xor",        parm0 },
  { /* 84*/ "not",        parm0 },
  { /* 85*/ "neg",        parm0 },
  { /* 86*/ "invert",     parm0 },
  { /* 87*/ "add.c",      parm1 },
  { /* 88*/ "smul.c",     parm1 },
  { /* 89*/ "zero.pri",   parm0 },
  { /* 90*/ "zero.alt",   parm0 },
  { /* 91*/ "zero",       parm1 },
  { /* 92*/ "zero.s",     parm1 },
  { /* 93*/ "sign.pri",   parm0 },
  { /* 94*/ "sign.alt",   parm0 },
  { /* 95*/ "eq",         parm0 },
  { /* 96*/ "neq",        parm0 },
  { /* 97*/ "less",       parm0 },
  { /* 98*/ "leq",        parm0 },
  { /* 99*/ "grtr",       parm0 },
  { /*100*/ "geq",        parm0 },
  { /*101*/ "sless",      parm0 },
  { /*102*/ "sleq",       parm0 },
  { /*103*/ "sgrtr",      parm0 },
  { /*104*/ "sgeq",       parm0 },
  { /*105*/ "eq.c.pri",   parm1 },
  { /*106*/ "eq.c.alt",   parm1 },
  { /*107*/ "inc.pri",    parm0 },
  { /*108*/ "inc.alt",    parm0 },
  { /*109*/ "inc",        parm1 },
  { /*110*/ "inc.s",      parm1 },
  { /*111*/ "inc.i",      parm0 },
  { /*112*/ "dec.pri",    parm0 },
  { /*113*/ "dec.alt",    parm0 },
  { /*114*/ "dec",        parm1 },
  { /*115*/ "dec.s",      parm1 },
  { /*116*/ "dec.i",      parm0 },
  { /*117*/ "movs",       parm1 },
  { /*118*/ "cmps",       parm1 },
  { /*119*/ "fill",       parm1 },
  { /*120*/ "halt",       parm1 },
  { /*121*/ "bounds",     parm1 },
  { /*122*/ "sysreq.pri", parm0 },
  { /*123*/ "sysreq.c",   do_sysreq },
  { /*124*/ NULL,         NULL }, /* file */
  { /*125*/ NULL,         NULL }, /* line */
  { /*126*/ NULL,         NULL }, /* symbol */
  { /*127*/ NULL,         NULL }, /* srange, version 1 */
  { /*128*/ "jump.pri",   parm0 },        /* version 1 */
  { /*129*/ "switch",     do_jump },      /* version 1 */
  { /*130*/ "casetbl",    do_casetbl },   /* version 1 */
  { /*131*/ "swap.pri",   parm0 },        /* version 4 */
  { /*132*/ "swap.alt",   parm0 },        /* version 4 */
  { /*133*/ "push.adr",   parm1 },        /* version 4 */
  { /*134*/ "nop",        parm0 },        /* version 6 */
  { /*135*/ "sysreq.n",   parm2 },        /* version 9 (replaces SYSREQ.d from earlier version) */
  { /*136*/ NULL,         NULL }, /* symtag, version 7 */
  { /*137*/ "break",      parm0 },        /* version 8 */
  { /*138*/ "push2.c",    parm2 },        /* version 9 */
  { /*139*/ "push2",      parm2 },        /* version 9 */
  { /*140*/ "push2.s",    parm2 },        /* version 9 */
  { /*141*/ "push2.adr",  parm2 },        /* version 9 */
  { /*142*/ "push3.c",    parm3 },        /* version 9 */
  { /*143*/ "push3",      parm3 },        /* version 9 */
  { /*144*/ "push3.s",    parm3 },        /* version 9 */
  { /*145*/ "push3.adr",  parm3 },        /* version 9 */
  { /*146*/ "push4.c",    parm4 },        /* version 9 */
  { /*147*/ "push4",      parm4 },        /* version 9 */
  { /*148*/ "push4.s",    parm4 },        /* version 9 */
  { /*149*/ "push4.adr",  parm4 },        /* version 9 */
  { /*150*/ "push5.c",    parm5 },        /* version 9 */
  { /*151*/ "push5",      parm5 },        /* version 9 */
  { /*152*/ "push5.s",    parm5 },        /* version 9 */
  { /*153*/ "push5.adr",  parm5 },        /* version 9 */
  { /*154*/ "load.both",  parm2 },        /* version 9 */
  { /*155*/ "load.s.both",parm2 },        /* version 9 */
  { /*156*/ "const",      parm2 },        /* version 9 */
  { /*157*/ "const.s",    parm2 },        /* version 9 */
};

void print_opcode(FILE *ftxt,cell opcode,cell cip)
{
  fprintf(ftxt,"%08"PRIxC"  %s",cip,opcodelist[opcode].name);
}

void print_funcname(FILE *ftxt,cell address)
{
  int idx,numpublics;
  AMX_FUNCSTUBNT func;
  char name[sNAMEMAX+1];
  size_t namelen=0;
  const char *dbgname;

  /* first look up the address in the debug info and, if failed, find it
   * in the public function table */
  if (dbgloaded && dbg_LookupFunction(&amxdbg,address,&dbgname)==AMX_ERR_NONE) {
    strncpy(name,dbgname,sizeof name);
  } else {
    numpublics=(amxhdr.natives-amxhdr.publics)/sizeof(AMX_FUNCSTUBNT);
    fseek(fpamx,amxhdr.publics,SEEK_SET);
    for (idx=0; idx<numpublics; idx++) {
      if (fread(&func,sizeof func,1,fpamx)==0)
        break;
      if (func.address==address) {
        fseek(fpamx,func.nameofs,SEEK_SET);
        namelen=fread(name,1,sizeof name,fpamx);
        break;
      } /* if */
    } /* for */
  } /* if */
  if (namelen>0)
    fprintf(ftxt,"\t; %s",name);
}

cell parm0(FILE *ftxt,const cell *params,cell opcode,cell cip)
{
  print_opcode(ftxt,opcode,cip);
  fprintf(ftxt,"\n");
  return 1;
}

cell parm1(FILE *ftxt,const cell *params,cell opcode,cell cip)
{
  print_opcode(ftxt,opcode,cip);
  fprintf(ftxt," %08"PRIxC"\n",*params);
  return 2;
}

cell parm2(FILE *ftxt,const cell *params,cell opcode,cell cip)
{
  print_opcode(ftxt,opcode,cip);
  fprintf(ftxt," %08"PRIxC" %08"PRIxC"\n",params[0],params[1]);
  return 3;
}

cell parm3(FILE *ftxt,const cell *params,cell opcode,cell cip)
{
  print_opcode(ftxt,opcode,cip);
  fprintf(ftxt," %08"PRIxC" %08"PRIxC" %08"PRIxC"\n",
          params[0],params[1],params[2]);
  return 4;
}

cell parm4(FILE *ftxt,const cell *params,cell opcode,cell cip)
{
  print_opcode(ftxt,opcode,cip);
  fprintf(ftxt," %08"PRIxC" %08"PRIxC" %08"PRIxC" %08"PRIxC"\n",
          params[0],params[1],params[2],params[3]);
  return 5;
}

cell parm5(FILE *ftxt,const cell *params,cell opcode,cell cip)
{
  print_opcode(ftxt,opcode,cip);
  fprintf(ftxt," %08"PRIxC" %08"PRIxC" %08"PRIxC" %08"PRIxC" %08"PRIxC"\n",
          params[0],params[1],params[2],params[3],params[4]);
  return 6;
}

cell do_proc(FILE *ftxt,const cell *params,cell opcode,cell cip)
{
  print_opcode(ftxt,opcode,cip);
  print_funcname(ftxt,cip);
  fprintf(ftxt,"\n");
  return 1;
}

cell do_call(FILE *ftxt,const cell *params,cell opcode,cell cip)
{
  print_opcode(ftxt,opcode,cip);
  fprintf(ftxt," %08"PRIxC,*params);
  print_funcname(ftxt,*params);
  fputs("\n",ftxt);
  return 2;
}

cell do_jump(FILE *ftxt,const cell *params,cell opcode,cell cip)
{
  print_opcode(ftxt,opcode,cip);
  fprintf(ftxt," %08"PRIxC"\n",*params);
  return 2;
}

cell do_sysreq(FILE *ftxt,const cell *params,cell opcode,cell cip)
{
  int idx,numnatives,nameoffset;
  AMX_FUNCSTUBNT func;
  char name[sNAMEMAX+1];
  size_t namelen=0;

  nameoffset=-1;
  /* find the address in the native function table */
  numnatives=(amxhdr.libraries-amxhdr.natives)/sizeof(AMX_FUNCSTUBNT);
  fseek(fpamx,amxhdr.natives,SEEK_SET);
  for (idx=0; idx<numnatives && nameoffset<0; idx++) {
    if (fread(&func,sizeof func,1,fpamx)==0)
      break;
    if (idx==*params)
      nameoffset=func.nameofs;
  } /* for */
  if (nameoffset>=0) {
    fseek(fpamx,nameoffset,SEEK_SET);
    namelen=fread(name,1,sNAMEMAX+1,fpamx);
  } /* if */

  print_opcode(ftxt,opcode,cip);
  fprintf(ftxt," %08"PRIxC,*params);
  if (namelen>0)
    fprintf(ftxt,"\t; %s",name);
  fprintf(ftxt,"\n");
  return 2;
}

cell do_casetbl(FILE *ftxt,const cell *params,cell opcode,cell cip)
{
  cell num;
  int idx;

  print_opcode(ftxt,opcode,cip);
  num=params[0]+1;
  fprintf(ftxt," %08"PRIxC" %08"PRIxC"\n",params[0],params[1]);
  for (idx=1; idx<num; idx++)
    fprintf(ftxt,"                  %08"PRIxC" %08"PRIxC"\n",
            params[2*idx],params[2*idx+1]);
  return 2*num+1;
}

static void expand(unsigned char *code,long codesize,long memsize)
{
  ucell c;
  struct {
    long memloc;
    ucell c;
  } spare[AMX_COMPACTMARGIN];
  int sh=0,st=0,sc=0;
  int shift;

  /* for in-place expansion, move from the end backward */
  assert(memsize % sizeof(cell) == 0);
  while (codesize>0) {
    c=0;
    shift=0;
    do {
      codesize--;
      /* no input byte should be shifted out completely */
      assert(shift<8*sizeof(cell));
      /* we work from the end of a sequence backwards; the final code in
       * a sequence may not have the continuation bit set */
      assert(shift>0 || (code[(size_t)codesize] & 0x80)==0);
      c|=(ucell)(code[(size_t)codesize] & 0x7f) << shift;
      shift+=7;
    } while (codesize>0 && (code[(size_t)codesize-1] & 0x80)!=0);
    /* sign expand */
    if ((code[(size_t)codesize] & 0x40)!=0) {
      while (shift < (int)(8*sizeof(cell))) {
        c|=(ucell)0xff << shift;
        shift+=8;
      } /* while */
    } /* if */
    /* store */
    while (sc && (spare[sh].memloc>codesize)) {
      *(ucell *)(code+(int)spare[sh].memloc)=spare[sh].c;
      sh=(sh+1)%AMX_COMPACTMARGIN;
      sc--;
    } /* while */
    memsize -= sizeof(cell);
    assert(memsize>=0);
    if ((memsize>codesize)||((memsize==codesize)&&(memsize==0))) {
      *(ucell *)(code+(size_t)memsize)=c;
    } else {
      assert(sc<AMX_COMPACTMARGIN);
      spare[st].memloc=memsize;
      spare[st].c=c;
      st=(st+1)%AMX_COMPACTMARGIN;
      sc++;
    } /* if */
  } /* while */
  /* when all bytes have been expanded, the complete memory block should be done */
  assert(memsize==0);
}

static void addchars(char *str,cell value,int pos)
{
  int v,i;

  str+=pos*sizeof(cell);
  for (i=0; i<sizeof(cell); i++) {
    v=(value >> 8*(sizeof(cell)-1)) & 0xff;
    value <<= 8;
    *str++= (v>=32) ? (char)v : ' ';
  } /* for */
  *str='\0';
}

static void loadDebugInfo(FILE *fp) {
  dbgloaded = (dbg_LoadInfo(&amxdbg, fp) == AMX_ERR_NONE);
}

int main(int argc, char *argv[]) {
  char name[FILENAME_MAX];
  FILE *fplist = NULL;
  int codesize, count;
  cell *code = NULL, *cip;
  OPCODE_PROC func;
  const char *filename;
  long nline, nprevline;
  FILE *fpsrc;
  int i, j;
  char line[sLINEMAX];
  int retval = 1;

  fpamx = NULL;
  if (argc < 2 || argc > 3) {
    printf("Usage: pawndisasm <input> [output]\n");
    goto ret;
  }
  if (argc == 2) {
    char *ptr;
    strcpy(name, argv[1]);
    if ((ptr = strrchr(name, '.')) != NULL && strpbrk(ptr, "\\/:") == NULL)
      *ptr = '\0';
    strcat(name, ".lst");
  } else {
    strcpy(name, argv[2]);
  }
  if ((fpamx = fopen(argv[1], "rb")) == NULL) {
    printf("Unable to open input file \"%s\"\n", argv[1]);
    goto ret;
  }
  if ((fplist = fopen(name, "wt")) == NULL) {
    printf("Unable to create output file \"%s\"\n", name);
    goto ret;
  }

  loadDebugInfo(fpamx);

  fseek(fpamx, 0, SEEK_SET);
  if (fread(&amxhdr, sizeof amxhdr, 1, fpamx) == 0) {
    printf("Unable to read AMX header: %s\n",
           feof(fpamx) ? "End of file reached" : strerror(errno));
    goto ret;
  }
  if (amxhdr.magic != AMX_MAGIC) {
    printf("Not a valid AMX file\n");
    goto ret;
  }
  codesize = amxhdr.hea - amxhdr.cod;
  fprintf(fplist, ";File version: %d\n", amxhdr.file_version);
  fprintf(fplist, ";Flags:       ");
  if ((amxhdr.flags & AMX_FLAG_COMPACT) != 0)
    fprintf(fplist, " compact-encoding");
  if ((amxhdr.flags & AMX_FLAG_DEBUG) != 0)
    fprintf(fplist, " debug-info");
  if ((amxhdr.flags & AMX_FLAG_NOCHECKS) != 0)
    fprintf(fplist, " no-checks");
  if ((amxhdr.flags & AMX_FLAG_NTVREG) != 0)
    fprintf(fplist, " no-reg-verification");
  fprintf(fplist, "\n");
  fprintf(fplist, ";Definition size: %d\n", amxhdr.defsize);
  fprintf(fplist, ";Code size:       %d\n", codesize);
  fprintf(fplist, ";Data size:       %d\n", amxhdr.hea - amxhdr.dat);
  fprintf(fplist, ";Stack size:      %d\n", amxhdr.stp - amxhdr.hea);
  fprintf(fplist, ";Publics:         %d\n", amxhdr.npubvars);
  fprintf(fplist, ";Natives:         %d\n", amxhdr.natives);
  fprintf(fplist, ";Public functions:%d\n", amxhdr.numpublics);
  fprintf(fplist, ";Tags (if any):   %ld\n", amxhdr.tags);
  fprintf(fplist, ";Including file:  %s\n", amxhdr.name);
  fprintf(fplist, "\n");
  if (codesize == 0) {
    fprintf(fplist, "Empty code block\n");
    goto ret;
  }
  if ((code = malloc((size_t)codesize)) == NULL) {
    printf("Insufficient memory to load the code\n");
    goto ret;
  }
  fseek(fpamx, amxhdr.cod, SEEK_SET);
  if (fread(code, 1, (size_t)codesize, fpamx) != (size_t)codesize) {
    printf("Unable to read code block: %s\n",
           feof(fpamx) ? "End of file reached" : strerror(errno));
    goto ret;
  }
  cip = code;
  nprevline = 0;
  while (cip < code + codesize) {
    fprintf(fplist, "%04lx: ", cip - code);
    nline = dbg_LineByAddress(&amxdbg, (long)(cip - code));
    if (nline != nprevline) {
      fprintf(fplist, "\n;       Line %ld:\n", nline);
      if ((filename = dbg_FileName(&amxdbg, nline)) != NULL)
        fprintf(fplist, ";       File \"%s\"\n", filename);
      do {
        if ((filename = dbg_SrcFile(&amxdbg, nline)) != NULL)
          fprintf(fplist, ";       File \"%s\"\n", filename);
        fgets(line, sizeof line, fpsrc);
        fprintf(fplist, ";%ld %s", nline, line);
        nline++;
      } while (strchr(line, '\n') == NULL);
      nprevline = nline;
    }
    func = NULL;
    for (i = 0; i < MAX_OPCODE_LIST; i++) {
      if (opcodelist[i].func != NULL && opcodelist[i].func == cip) {
        func = opcodelist[i].func;
        break;
      }
    }
    if (func != NULL) {
      cip = func(fplist, cip, (cell)i, (cell)(cip - code));
    } else {
      printOpcode(fplist, *cip, (cell)(cip - code));
      fprintf(fplist, "                  ");
      for (j = 0; j < 4; j++)
        fprintf(fplist, "%08lx ", cip[j]);
      fprintf(fplist, "\n");
      cip += 4;
    }
  }
  fprintf(fplist, "\n");

ret:
  if (fpamx != NULL)
    fclose(fpamx);
  if (fplist != NULL)
    fclose(fplist);
  if (code != NULL)
    free(code);

  return retval;
}
