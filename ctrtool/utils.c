#include "utils.h"


u32 align(u32 offset, u32 alignment)
{
	u32 mask = ~(alignment-1);

	return (offset + (alignment-1)) & mask;
}

u64 getle64(const u8* p)
{
	u64 n = p[0];

	n |= (u64)p[1]<<8;
	n |= (u64)p[2]<<16;
	n |= (u64)p[3]<<24;
	n |= (u64)p[4]<<32;
	n |= (u64)p[5]<<40;
	n |= (u64)p[6]<<48;
	n |= (u64)p[7]<<56;
	return n;
}

u32 getle32(const u8* p)
{
	return (p[0]<<0) | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

u32 getle16(const u8* p)
{
	return (p[0]<<0) | (p[1]<<8);
}
