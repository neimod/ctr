#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;


enum flags
{
	ExtractFlag = (1<<0),
	InfoFlag = (1<<1),
	PlainFlag = (1<<2),
	VerboseFlag = (1<<3),
};

#endif
