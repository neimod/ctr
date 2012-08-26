#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "types.h"
#include "filepath.h"

void filepath_init(filepath* fpath)
{
	fpath->valid = 0;
}

void filepath_copy(filepath* fpath, filepath* copy)
{
	if (copy != 0 && copy->valid)
		memcpy(fpath, copy, sizeof(filepath));
	else
		memset(fpath, 0, sizeof(filepath));
}

void filepath_append_utf16(filepath* fpath, const u8* name)
{
	u32 size;


	if (fpath->valid == 0)
		return;

	size = strlen(fpath->pathname);

	if (size > 0 && size < (MAX_PATH-1))
	{
		if (fpath->pathname[size-1] != PATH_SEPERATOR)
			fpath->pathname[size++] = PATH_SEPERATOR;
	}

	while(size < (MAX_PATH-1))
	{
		u8 lo = *name++;
		u8 hi = *name++;
		u16 code = (hi<<8) | lo;

		if (code == 0)
			break;

		// convert non-ANSI to '#', because unicode support is too much work
		if (code > 0x7F)
			code = '#';
		
		fpath->pathname[size++] = code;
	}

	fpath->pathname[size] = 0;

	if (size >= (MAX_PATH-1))
		fpath->valid = 0;
}

void filepath_append(filepath* fpath, const char* format, ...)
{
	char tmppath[MAX_PATH];
	va_list args;

	if (fpath->valid == 0)
		return;

	memset(tmppath, 0, MAX_PATH);
    
	va_start(args, format);
    vsprintf(tmppath, format, args);
    va_end(args);

	strcat(fpath->pathname, "/");
	strcat(fpath->pathname, tmppath);
}

void filepath_set(filepath* fpath, const char* path)
{
	fpath->valid = 1;
	memset(fpath->pathname, 0, MAX_PATH);
	strncpy(fpath->pathname, path, MAX_PATH);
}

const char* filepath_get(filepath* fpath)
{
	if (fpath->valid == 0)
		return 0;
	else
		return fpath->pathname;
}
