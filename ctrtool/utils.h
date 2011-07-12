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

#ifdef __cplusplus
}
#endif

#endif // _UTILS_H_
