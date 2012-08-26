#ifndef _FILEPATH_H_
#define _FILEPATH_H_

#include "types.h"
#include "utils.h"

typedef struct
{
	char pathname[MAX_PATH];
	int valid;
} filepath;

void filepath_init(filepath* fpath);
void filepath_copy(filepath* fpath, filepath* copy);
void filepath_append_utf16(filepath* fpath, const u8* name);
void filepath_append(filepath* fpath, const char* format, ...);
void filepath_set(filepath* fpath, const char* path);
const char* filepath_get(filepath* fpath);

#endif // _FILEPATH_H_
