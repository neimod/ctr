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
void filepath_set(filepath* fpath, const char* path);
const char* filepath_get(filepath* fpath);

#endif // _FILEPATH_H_
