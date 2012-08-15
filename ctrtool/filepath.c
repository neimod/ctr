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
