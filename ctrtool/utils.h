#ifndef _UTILS_H_
#define _UTILS_H_


#include "types.h"


#ifdef __cplusplus
extern "C" {
#endif

u32 align(u32 offset, u32 alignment);
u64 getle64(const u8* p);
u32 getle32(const u8* p);
u32 getle16(const u8* p);
u32 getbe32(const u8* p);
u32 getbe16(const u8* p);

void readkeyfile(u8* key, const char* keyfname);
void memdump(FILE* fout, const char* prefix, const u8* data, u32 size);
void hexdump(void *ptr, int buflen);

#ifdef __cplusplus
}
#endif

#endif // _UTILS_H_
