/*
 * main.c
 */

#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "types.h"
#include "absyn.h"
#include "errormsg.h"
#include "temp.h" /* needed by translate.h */
#include "tree.h" /* needed by frame.h */
#include "assem.h"
#include "frame.h" /* needed by translate.h and printfrags prototype */
#include "translate.h"
#include "env.h"
#include "semant.h" /* function prototype for transProg */
#include "canon.h"
#include "prabsyn.h"
#include "printtree.h"
#include "escape.h" /* needed by escape analysis */
#include "parse.h"
#include "codegen.h"
#include "regalloc.h"

extern bool anyErrors;

/* print the assembly language instructions to filename.s */
static void doProc(FILE *out, F_frame frame, T_stm body)
{
 AS_proc proc;
 T_stmList stmList;
 AS_instrList iList;

 F_tempMap = Temp_empty();
 F_initRegs();

 stmList = C_linearize(body);
 stmList = C_traceSchedule(C_basicBlocks(stmList));

 printStmList(stdout, stmList);

 iList  = F_codegen(frame, stmList); /* 9 */

 struct RA_result ra = RA_regAlloc(frame, iList);  /* 10, 11 */

 fprintf(out, "#BEGIN function\n");
 proc = F_procEntryExit3(frame, iList);
 fprintf(out, "%s", proc->prolog);
 AS_printInstrList (out, proc->body,
                       Temp_layerMap(F_tempMap,ra.coloring)); // uncomment this to continue
//                       Temp_layerMap(F_tempMap,Temp_name())); // for debug
 fprintf(out, "%s", proc->epilog);
 fprintf(out, "#END function\n\n");
}

int main(int argc, string *argv)
{
 A_exp absyn_root;
 S_table base_env, base_tenv;
 F_fragList frags;
 char outfile[100];
 FILE *out = stdout;

 if (argc==2) {
   absyn_root = parse(argv[1]);
   if (!absyn_root)
     return 1;
     
#if 0
   pr_exp(out, absyn_root, 0); /* print absyn data structure */
   fprintf(out, "\n");
#endif
	//If you have implemented escape analysis, uncomment this
   Esc_findEscape(absyn_root); /* set varDec's escape field */

   frags = SEM_transProg(absyn_root);
   if (anyErrors) return 1; /* don't continue */

   /* convert the filename */
   sprintf(outfile, "%s.s", argv[1]);
   out = fopen(outfile, "w");
   /* Chapter 8, 9, 10, 11 & 12 */
   for (;frags;frags=frags->tail) {
     if (frags->head->kind == F_procFrag) {
       static int head = 1;
       if (head) { 
         fprintf(out, ".text\n"); 
          fprintf(out, ".global tigermain\n");
          fprintf(out, ".type tigermain, @function\n");
         head = 0; }

       doProc(out, frags->head->u.proc.frame, frags->head->u.proc.body);
     } else if (frags->head->kind == F_stringFrag) {
       static int head = 1; 
       if (head) { fprintf(out, ".section .rodata\n"); head = 0; }

       F_frag f = F_string(frags->head->u.stringg.label, frags->head->u.stringg.str);
       fprintf(out, "%s:\n.string \"", S_name(f->u.stringg.label));
       int len = *(int*)f->u.stringg.str;
       fwrite(f->u.stringg.str, 1, len+4, out);
       fprintf(out, "\"\n");
     }
   }

   fclose(out);
   return 0;
 }
 EM_error(0,"usage: tiger file.tig");
 return 1;
}
