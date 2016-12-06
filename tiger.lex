%{
#include <string.h>
#include "util.h"
#include "symbol.h"
#include "errormsg.h"
#include "absyn.h"
#include "y.tab.h"

int charPos=1;

int yywrap(void)
{
 charPos=1;
 return 1;
}

void adjust(void)
{
 EM_tokPos=charPos;
 charPos+=yyleng;
}
/*
* Please don't modify the lines above.
* You can add C declarations of your own below.
*/

int commCnt = 0;

char *strbuf;
int strLen, bufLen, startPos;

void strInit()
{
	bufLen = 4;
	strbuf = checked_malloc(bufLen); /* remember to free.*/
	bzero(strbuf, bufLen);
	strbuf[0]='\0';
	strLen = 0;
}

void strAppend(const char *s) 
{
	int newLen = strlen(s);
	strLen += newLen; // new str length
	if (strLen + 1 /* for last '\0' */ > bufLen) {
		while (bufLen < (strLen + 1)) { bufLen *= 2; }
		
		char *newbuf = checked_malloc(bufLen);
		strcpy(newbuf, strbuf);
		free(strbuf);
		strbuf = newbuf; // new strbuf
	}
	strcat(strbuf, s);
}

void strFinish()
{
	startPos = 0;
	free(strbuf);
}

void checkNewline(char *text) {
	int len = strlen(text), i;
	for (i = 0; i < len; ++i) {
		if (text[i] == '\n') EM_newline();
	}
}

%}
  /* You can add lex defINITIALions here. */
digits [0-9]+
id ([a-z]|[A-Z])+([0-9]|"_"|[a-z]|[A-Z])*
space " "|"\t"
formats " "|"\t"|"\n"|"\f"


  /* 
  * Below are some examples, which you can wipe out
  * and write reguler expressions and actions of your own.
  */ 
%s COMMENT STR 
%%

<INITIAL>{
array           { adjust(); return ARRAY;    }
if              { adjust(); return IF;       }
then            { adjust(); return THEN;     }
else            { adjust(); return ELSE;     }
for             { adjust(); return FOR;      }
to              { adjust(); return TO;       }
while           { adjust(); return WHILE;    }
do              { adjust(); return DO;       }
let             { adjust(); return LET;      }
in              { adjust(); return IN;       }
end             { adjust(); return END;      }
of              { adjust(); return OF;       }
break           { adjust(); return BREAK;    }
nil             { adjust(); return NIL;      }
function        { adjust(); return FUNCTION; }
var             { adjust(); return VAR;      }
type            { adjust(); return TYPE;     }
}

<INITIAL>{
{id}     { adjust(); yylval.sval=String(yytext); return ID; }
{digits} { adjust(); yylval.ival=atoi(yytext); return INT;  }
{space}  { adjust(); continue;                              }
","             { adjust(); return COMMA;     }
":="            { adjust(); return ASSIGN;    }
":"             { adjust(); return COLON;     }
";"             { adjust(); return SEMICOLON; }
"("             { adjust(); return LPAREN;    }
")"             { adjust(); return RPAREN;    }
"["             { adjust(); return LBRACK;    }
"]"             { adjust(); return RBRACK;    }
"{"             { adjust(); return LBRACE;    }
"}"             { adjust(); return RBRACE;    }
"."             { adjust(); return DOT;       }
"+"             { adjust(); return PLUS;      }
"-"             { adjust(); return MINUS;     }
"*"             { adjust(); return TIMES;     }
"/"             { adjust(); return DIVIDE;    }
"="             { adjust(); return EQ;        }
"<>"            { adjust(); return NEQ;       }
"<"             { adjust(); return LT;        }
"<="            { adjust(); return LE;        }
">"             { adjust(); return GT;        }
">="            { adjust(); return GE;        }
"&"             { adjust(); return AND;       }
"|"             { adjust(); return OR;        }
""              { adjust(); return OR;        }
}

<INITIAL>\" { 
	adjust();
	BEGIN STR;
	strInit();
	startPos = EM_tokPos;
	}

<STR>{
\\n  { adjust(); strAppend("\n");}
\\t  { adjust(); strAppend("\t");}
\\\\ { adjust(); strAppend("\\");}
\\([0-2])([0-9])([0-9]) {
	adjust(); 
	char c[2];
	/* Maybe check c > 255? */
	c[0] = (yytext[1]-'0') * 100 + (yytext[2]-'0') * 10 + (yytext[3]-'0');
	c[1] = '\0';
	strAppend(c);
	}
\" {
	adjust(); 
	if (strLen > 0) {
		yylval.sval = String(strbuf);
	} else {
		yylval.sval = "";
	}
	EM_tokPos = startPos;
	strFinish() ;
	BEGIN INITIAL; 
	return STRING; 
	}
\\{formats}+\\ {adjust(); checkNewline(yytext);}
\\\. {adjust(); EM_error(EM_tokPos,"illegal token");}
. {adjust(); strAppend(yytext);}
}

<INITIAL>"/*" { adjust(); BEGIN COMMENT; commCnt++;            }
<INITIAL>"\n" { adjust(); EM_newline(); continue;              }
<INITIAL>.    { adjust(); EM_error(EM_tokPos,"illegal token"); }
<COMMENT>{
"\n" { adjust(); EM_newline(); continue;   }
"*/" { adjust(); BEGIN INITIAL; commCnt++; }
.    { adjust();                           }
}

%%
//.               { BEGIN INITIAL; yyless(1);                      }
