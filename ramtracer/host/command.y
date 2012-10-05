%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hw_command.h"
#include "hw_commandbuffer.h"
#include "hw_commandtype.h"
#include "hw_main.h"


extern int yylex(void* ctx);
extern int yyparse(void* ctx);
extern FILE *yyin;


static HWMemoryPool* getpool(void* ctx)
{
	HWCommand* command = (HWCommand*)ctx;
	
	return &command->mempool;
}

static HWCommand* getcmd(void* ctx)
{
	return (HWCommand*)ctx;
}
 
void yyerror(const char *s);
#define YYPARSE_PARAM ctx
#define YYLEX_PARAM ctx

%}


%union {
	HWCommandNumber num;
	HWCommandBuffer* buf;
	HWCommandString* string;
	HWCommandWideString* wstring;
	HWCommandType* cmdtype;
}

%token <num> NUMBER
%token <string> STRING
%token <wstring> WSTRING
%token CMDREADBYTE
%token CMDREADSHORT
%token CMDREADLONG
%token CMDREADMEM
%token CMDREADMEMTOFILE
%token CMDWRITEBYTE
%token CMDWRITESHORT
%token CMDWRITELONG
%token CMDWRITEMEM
%token CMDWRITEFILE
%token CMDQUIT
%token CMDPXI
%token CMDMEMSET
%token CMDBREAKPOINT
%token CMDCONTINUE
%token CMDSETEXCEPTION
%token DABORT
%token PABORT
%token ENDLINE
%token LBRACKET
%token RBRACKET
%token UNKNOWN

%type <num> number
%type <buf> bytedata
%type <buf> bracketedbytedata
%type <string> string
%type <wstring> wstring
%type <cmdtype> datatype
%type <cmdtype> datatypes
%type <cmdtype> nonemptydatatypes

%start stmt

%%
stmt: 		CMDREADBYTE number ENDLINE    						{ HW_CommandReadByte(getcmd(ctx), $2.value); YYACCEPT; }
			| CMDREADSHORT number ENDLINE    					{ HW_CommandReadShort(getcmd(ctx), $2.value); YYACCEPT; }
			| CMDREADLONG number ENDLINE    					{ HW_CommandReadLong(getcmd(ctx), $2.value); YYACCEPT;  }
			| CMDREADMEMTOFILE number number string ENDLINE    	{ HW_CommandReadMemToFile(getcmd(ctx), $2.value, $3.value, $4); YYACCEPT;  }
			| CMDREADMEM number number ENDLINE    				{ HW_CommandReadMem(getcmd(ctx), $2.value, $3.value); YYACCEPT;  }
			| CMDWRITEBYTE number number ENDLINE    			{ HW_CommandWriteByte(getcmd(ctx), $2.value, $3.value); YYACCEPT; }
			| CMDWRITESHORT number number ENDLINE    			{ HW_CommandWriteShort(getcmd(ctx), $2.value, $3.value); YYACCEPT; }
			| CMDWRITELONG number number ENDLINE    			{ HW_CommandWriteLong(getcmd(ctx), $2.value, $3.value); YYACCEPT; }
			| CMDWRITEMEM number bracketedbytedata ENDLINE    	{ HW_CommandWriteMem(getcmd(ctx), $2.value, $3); YYACCEPT; }
			| CMDWRITEFILE number string ENDLINE    			{ HW_CommandWriteFile(getcmd(ctx), $2.value, $3); YYACCEPT; }
			| CMDQUIT ENDLINE   					 			{ HW_RequestExit(); YYACCEPT; }
			| CMDMEMSET number number number ENDLINE   			{ HW_CommandMemset(getcmd(ctx), $2.value, $3.value, $4.value); YYACCEPT; }
			| CMDSETEXCEPTION DABORT number ENDLINE   			{ HW_CommandSetExceptionDataAbort(getcmd(ctx), $3.value); YYACCEPT; }
			| CMDPXI number number datatypes ENDLINE   			{ HW_CommandPxi(getcmd(ctx), $2.value, $3.value, $4); YYACCEPT; }
			| CMDBREAKPOINT number ENDLINE   					{ HW_CommandSetBreakpoint(getcmd(ctx), $2.value); YYACCEPT; }
			| CMDCONTINUE ENDLINE   							{ HW_CommandContinue(getcmd(ctx)); YYACCEPT; }
			| ENDLINE 											{ YYACCEPT; }
			| error ENDLINE										{ YYABORT; }
			;
			
datatypes:	/* empty */											{ $$ = 0; }
			| nonemptydatatypes									{ $$ = $1; }
			;

nonemptydatatypes: nonemptydatatypes datatype					{ $$ = HW_CommandTypeAppend($1, $2); }
			| datatype											{ $$ = $1; }
			;
			
datatype:	number												{ $$ = HW_CommandTypeAllocWithNumber(getpool(ctx), &$1); }
			| bracketedbytedata									{ $$ = HW_CommandTypeAllocWithBuffer(getpool(ctx), $1); }
			;

bracketedbytedata: LBRACKET bytedata RBRACKET					{ $$ = $2; }
			;

bytedata: 	bytedata number  									{ $$ = HW_CommandBufferAppendNumber($1, &$2); }
			| bytedata string  									{ $$ = HW_CommandBufferAppendString($1, $2); }
			| bytedata wstring  								{ $$ = HW_CommandBufferAppendWideString($1, $2); }
			| number											{ $$ = HW_CommandBufferAllocWithNumber(getpool(ctx), &$1); }
			| string											{ $$ = HW_CommandBufferAllocWithString(getpool(ctx), $1); }			
			| wstring											{ $$ = HW_CommandBufferAllocWithWideString(getpool(ctx), $1); }			
			;

string: 	STRING  											{ $$ = $1; }
			;

wstring: 	WSTRING  											{ $$ = $1; }
			;

number: 	NUMBER  		 									{ $$ = $1; }
			;
%%

void HW_CommandParse(HWCommand* command) {
	HW_MemoryPoolClear(&command->mempool); 
	yyparse(command); 
	HW_MemoryPoolClear(&command->mempool); 
}

void yyerror(const char *s) {
	printf("%s\n", s);
}