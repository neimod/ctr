%{
#include <unistd.h>

#define YY_DECL extern int yylex(void* ctx)

#include "hw_command.h"
#include "hw_commandbuffer.h"
#include "hw_commandtype.h"
#include "commandparser.h"


unsigned int getnumtype(const char* text)
{
	unsigned int textlen = strlen(text);
	
	if (textlen <= 2)
		return NUMTYPE_BYTE;
	else if (textlen <= 4)
		return NUMTYPE_SHORT;
	else
		return NUMTYPE_LONG;
}

HWMemoryPool* getpool(void* ctx)
{
	HWCommand* command = (HWCommand*)ctx;
	
	return &command->mempool;
}

HWCommandString* getstring(void* ctx, const char* text)
{
	HWMemoryPool* pool = getpool(ctx);
	unsigned int length = strlen(text+1);
	
	HWCommandString* string = HW_MemoryPoolAlloc(pool, sizeof(HWCommandString));
	if (string == 0)
		goto clean;
		
	string->value = HW_MemoryPoolAlloc(pool, length);
	if (string->value == 0)
		goto clean;
		
	memset(string->value, 0, length);
	memcpy(string->value, text+1, length-1);
	string->length = length-1;
	return string;
clean:
	if (string)
	{
		if (string->value)
			HW_MemoryPoolFree(pool, string->value);
		HW_MemoryPoolFree(pool, string);
	}
	return 0;
}

HWCommandWideString* getwstring(void* ctx, const char* text)
{
	HWMemoryPool* pool = getpool(ctx);
	unsigned int length = strlen(text+1);
	unsigned int i;
	
	HWCommandWideString* wstring = HW_MemoryPoolAlloc(pool, sizeof(HWCommandWideString));
	if (wstring == 0)
		goto clean;
		
	wstring->value = HW_MemoryPoolAlloc(pool, sizeof(short)*length);
	if (wstring->value == 0)
		goto clean;
		
	memset(wstring->value, 0, sizeof(short)*length);		
	for(i=0; i<length-1; i++)
		wstring->value[i] = text[1+i];
	wstring->length = length-1;	
	return wstring;
clean:
	if (wstring)
	{
		if (wstring->value)
			HW_MemoryPoolFree(pool, wstring->value);
		HW_MemoryPoolFree(pool, wstring);
	}
	return 0;	
}

#ifdef __MACH__ 
	#define READLINE
#endif

#ifdef READLINE

#include <readline/readline.h>
#include <readline/history.h>

/* Support for the readline and history libraries.  This allows
   nicer input on the interactive part of input. */

/* Have input call the following function. */
#undef  YY_INPUT
#define YY_INPUT(buf,result,max_size) \
		rl_input((char *)buf, &result, max_size)

/* Variables to help interface readline with bc. */
static char *rl_line = (char *)NULL;
static char *rl_start = (char *)NULL;
static int   rl_len = 0;

/* Definitions for readline access. */

/* rl_input puts upto MAX characters into BUF with the number put in
   BUF placed in *RESULT.  If the yy input file is the same as
   rl_instream (stdin), use readline.  Otherwise, just read it.
*/

static void
rl_input (buf, result, max)
	char *buf;
	int  *result;
	int   max;
{
  if (yyin != stdin)
    {
      while ( (*result = read( fileno(yyin), buf, max )) < 0 )
        if (errno != EINTR)
	  {
	    yyerror( "read() in flex scanner failed" );
	    exit (1);
	  }
      return;
    }

  /* Do we need a new string? */
  if (rl_len == 0)
    {
      if (rl_start)
	free(rl_start);
	rl_catch_signals = 0;
      rl_start = readline ("");
      if (rl_start == NULL) {
	/* end of file */
	*result = 0;
	rl_len = 0;
	return;
      }
      rl_line = rl_start;
      rl_len = strlen (rl_line)+1;
      if (rl_len != 1)
	add_history (rl_line); 
      rl_line[rl_len-1] = '\n';
      fflush (stdout);
    }

  if (rl_len <= max)
    {
      strncpy (buf, rl_line, rl_len);
      *result = rl_len;
      rl_len = 0;
    }
  else
    {
      strncpy (buf, rl_line, max);
      *result = max;
      rl_line += max;
      rl_len -= max;
    }
}
#endif
%}
%option noyywrap
%%
[ \t]           		;
[\n]					{ return ENDLINE; }
0x[0-9a-fA-F]+  		{ yylval.num.value=strtoul(yytext, 0, 0); yylval.num.type=getnumtype(yytext+2); return NUMBER; }
[0-9]+           	 	{ yylval.num.value=strtoul(yytext, 0, 0); yylval.num.type=NUMTYPE_LONG; return NUMBER; }
read8              		{ return CMDREADBYTE; }
read16              	{ return CMDREADSHORT; }
read32              	{ return CMDREADLONG; }
readmem              	{ return CMDREADMEM; }
readmemtofile           { return CMDREADMEMTOFILE; }
write8              	{ return CMDWRITEBYTE; }
write16              	{ return CMDWRITESHORT; }
write32              	{ return CMDWRITELONG; }
writemem              	{ return CMDWRITEMEM; }
writefile              	{ return CMDWRITEFILE; }
quit 	             	{ return CMDQUIT; }
pxi 	             	{ return CMDPXI; }
memset 	             	{ return CMDMEMSET; }
bp 	        			{ return CMDBREAKPOINT; }
c						{ return CMDCONTINUE; }
setexception 	        { return CMDSETEXCEPTION; }
dabort 		  	    	{ return DABORT; }
\"[^\"\n]+\"			{ yylval.string = getstring(ctx, yytext); return STRING; }
\`[^\`\n]+\`			{ yylval.wstring = getwstring(ctx, yytext); return WSTRING; }
\[						{ return LBRACKET; }
\]						{ return RBRACKET; }
.                 		{ return UNKNOWN; }
%%
int testlex() {
	// lex through the input:
	while(1) {yylex(0); }
}