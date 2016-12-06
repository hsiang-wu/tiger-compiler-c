%{
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "symbol.h" 
#include "errormsg.h"
#include "absyn.h"

int yylex(void); /* function prototype */

A_exp absyn_root;

void yyerror(char *s)
{
 EM_error(EM_tokPos, "%s", s);
 exit(1);
}
%}


%union {
	int pos;
	int ival;
	string sval;
	A_var var;
	A_exp exp;
	A_dec dec;
	A_decList decs;
	A_expList exps;
	A_ty ty;
	S_symbol symbol;
	/* et cetera */
	A_fieldList fields;
    A_efieldList efields;
    A_fundec fundec;
    A_fundecList fundecs;
    A_namety namety;
	A_nametyList nametys;
	}

%token <sval> ID STRING
%token <ival> INT

%token 
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK 
  LBRACE RBRACE DOT 
  PLUS MINUS TIMES DIVIDE EQ NEQ LT LE GT GE
  AND OR ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF 
  BREAK NIL
  FUNCTION VAR TYPE 

%type <exp> exp program expseq
%type <var> lvalue
/* declarations */
%type <dec> dec vardec
%type <decs> decs 
%type <fundec> fundec
%type <fundecs> fundecs
%type <namety> tydec
%type <nametys> tydecs
%type <ty> ty
/* et cetera */
%type <exps> explist paras
%type <fields> tyfields
%type <efields> efields

%start program

%nonassoc DO OF
%nonassoc ASSIGN

%left LBRACK
%nonassoc THEN
%nonassoc ELSE

%left RPAREN

/* precedence of operators */
%left OR
%left AND
%nonassoc EQ NEQ LT GT LE GE
%left PLUS MINUS
%left TIMES DIVIDE
%left UMINUS

/* to eliminate shift-reduce */

%%

/* Follow the sequence in APPENDIX */
program:   exp    {absyn_root=$1;}

/* 
 * A.2 
 * DECLARATIONS 
 */
decs: dec decs { $$=A_DecList($1,$2);}
	| dec      { $$=A_DecList($1, NULL);}

dec: tydecs    { $$=A_TypeDec(EM_tokPos, $1);}
   | vardec    { $$=$1;}
   | fundecs   { $$=A_FunctionDec(EM_tokPos, $1);}

/* data types */
tydec: TYPE ID EQ ty       { $$=A_Namety(S_Symbol($2), $4);}

tydecs: tydec              { $$=A_NametyList($1,NULL);}
      | tydec tydecs       { $$=A_NametyList($1, $2);}

ty: ID                     { $$=A_NameTy(EM_tokPos, S_Symbol($1));}
  | LBRACE tyfields RBRACE { $$=A_RecordTy(EM_tokPos, $2);}
  | ARRAY OF ID            { $$=A_ArrayTy(EM_tokPos, S_Symbol($3));}

tyfields:/* can be empty */ {$$=NULL;}
		| ID COLON ID {$$=A_FieldList(A_Field(EM_tokPos, S_Symbol($1), S_Symbol($3)), NULL);}
		| ID COLON ID COMMA tyfields
		{$$=A_FieldList(A_Field(EM_tokPos, S_Symbol($1), S_Symbol($3)), $5);}


/* variables */
vardec: VAR ID ASSIGN exp {$$=A_VarDec(EM_tokPos, S_Symbol($2), NULL, $4);}
	  | VAR ID COLON ID ASSIGN exp {$$=A_VarDec(EM_tokPos, S_Symbol($2), S_Symbol($4), $6);}

/* functions */
fundec: FUNCTION ID LPAREN tyfields RPAREN EQ exp 
	  {$$= A_Fundec(EM_tokPos, S_Symbol($2), $4, NULL, $7);}
	  | FUNCTION ID LPAREN tyfields RPAREN COLON ID EQ exp
	  {$$= A_Fundec(EM_tokPos, S_Symbol($2), $4, S_Symbol($7), $9);}

fundecs: fundec {$$=A_FundecList($1,NULL);}
       | fundec fundecs {$$=A_FundecList($1, $2);}

/* 
 * A.3 
 * VARIABLES AND EXPRESSIONS
 */
lvalue: ID {$$=A_SimpleVar(EM_tokPos, S_Symbol($1));}
      | lvalue DOT ID {$$=A_FieldVar(EM_tokPos, $1, S_Symbol($3));}
      /* 
        have a shift-reduce conflict with array creation: tid [exp] of exp,
        so move it to exp.
      | lvalue LBRACK exp RBRACK {$$=A_SubscriptVar(EM_tokPos, $1, $3);}
      */
/*exp: ID LBRACK exp RBRACK {$$=A_VarExp(EM_tokPos, A_SubscriptVar(EM_tokPos, 
   A_SimpleVar(EM_tokPos, S_Symbol($1)), $3));} */
lvalue: ID LBRACK exp RBRACK {$$=A_SubscriptVar(EM_tokPos, 
   A_SimpleVar(EM_tokPos, S_Symbol($1)), $3);}
lvalue: lvalue DOT ID LBRACK exp RBRACK {$$=A_SubscriptVar(EM_tokPos, 
   A_FieldVar(EM_tokPos, $1, S_Symbol($3)), $5);}

/* 
 * A.3
 * EXPRESSIONS 
 */

/* l-value */
exp: lvalue {$$=A_VarExp(EM_tokPos, $1);}

/* valueless expressions */
	/* procedure calls */
	/* break */
    exp: BREAK {$$=A_BreakExp(EM_tokPos);}

/* nil */
exp: NIL {$$=A_NilExp(EM_tokPos);}
/* sequencing */
exp: LPAREN explist RPAREN {$$=A_SeqExp(EM_tokPos, $2);}

/* no vallue */
/* integer literal */
exp: INT {$$=A_IntExp(EM_tokPos, $1);} 

/* string literal */
exp: STRING {$$=A_StringExp(EM_tokPos, $1);}

/* funciton call */
exp: ID LPAREN RPAREN {$$=A_CallExp(EM_tokPos, S_Symbol($1), NULL);}
   | ID LPAREN paras RPAREN {$$=A_CallExp(EM_tokPos, S_Symbol($1), $3);}

paras: {$$=NULL;}
     | exp {$$=A_ExpList($1, NULL);}
	 | exp COMMA paras {$$=A_ExpList($1, $3);}

/* arithmetic */
exp: exp PLUS exp           { $$=A_OpExp(EM_tokPos, A_plusOp, $1, $3);}
   | exp MINUS exp          { $$=A_OpExp(EM_tokPos, A_minusOp, $1, $3);}
   | exp TIMES exp          { $$=A_OpExp(EM_tokPos, A_timesOp, $1, $3);}
   | exp DIVIDE exp         { $$=A_OpExp(EM_tokPos, A_divideOp, $1, $3);}
   | MINUS exp %prec UMINUS { $$=A_OpExp(EM_tokPos, A_minusOp, A_IntExp(0, 0), $2);}

/* comparison */
exp: exp EQ exp {$$=A_OpExp(EM_tokPos, A_eqOp, $1, $3);}
   | exp NEQ exp {$$=A_OpExp(EM_tokPos, A_neqOp, $1, $3);}
   | exp GT exp {$$=A_OpExp(EM_tokPos, A_gtOp, $1, $3);}
   | exp LT exp {$$=A_OpExp(EM_tokPos, A_ltOp, $1, $3);}
   | exp GE exp {$$=A_OpExp(EM_tokPos, A_geOp, $1, $3);}
   | exp LE exp {$$=A_OpExp(EM_tokPos, A_leOp, $1, $3);}

/* string comparison */
/* boolean operators */
exp: exp AND exp {$$=A_IfExp(EM_tokPos, $1, $3, A_IntExp(EM_tokPos, 0));}
   | exp OR exp  {$$=A_IfExp(EM_tokPos, $1, A_IntExp(EM_tokPos, 1), $3);}

/* record creation */
exp: ID LBRACE efields RBRACE {$$=A_RecordExp(EM_tokPos, S_Symbol($1), $3);}

efields: {$$=NULL;}
      | ID EQ exp {$$=A_EfieldList(A_Efield(S_Symbol($1), $3), NULL);}
      | ID EQ exp COMMA efields {$$=A_EfieldList(A_Efield(S_Symbol($1), $3), $5);}

/* array creation */
exp: ID LBRACK exp RBRACK OF exp {$$=A_ArrayExp(EM_tokPos, S_Symbol($1), $3, $6);}

/* if-then-else */
exp: IF exp THEN exp ELSE exp {$$=A_IfExp(EM_tokPos, $2, $4, $6);}

/* if-then : change else to NULL*/
exp: IF exp THEN exp {$$=A_IfExp(EM_tokPos, $2, $4, NULL);}

/* assignment */
exp: lvalue ASSIGN exp {$$=A_AssignExp(EM_tokPos, $1, $3);}

/* while */
exp: WHILE exp DO exp {$$=A_WhileExp(EM_tokPos, $2, $4);}

/* for */
exp: FOR ID ASSIGN exp TO exp DO exp 
   {$$=A_ForExp(EM_tokPos, S_Symbol($2), $4, $6, $8);}

/* let */
exp: LET decs IN expseq END {
		$$=A_LetExp(EM_tokPos, $2, $4);
   } 

expseq: explist {$$=A_SeqExp(EM_tokPos, $1);} 

explist: {$$=NULL;}
       | exp {$$=A_ExpList($1, NULL);}
	   | exp SEMICOLON explist {$$=A_ExpList($1, $3);}

/* parentheses */
exp: LPAREN expseq RPAREN {$$=$2;}
