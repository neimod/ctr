#include <stdio.h>
#include <string.h>

#include "types.h"
#include "filepath.h"

void filepath_init(filepath* fpath)
{
	fpath->valid = 0;
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
