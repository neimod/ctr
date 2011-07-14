#ifndef _KEYSET_H_
#define _KEYSET_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	unsigned char n[256];
	unsigned char e[3];
	unsigned char d[256];
	unsigned char p[128];
	unsigned char q[128];
	unsigned char dp[128];
	unsigned char dq[128];
	unsigned char qp[128];
	int valid;
} rsakey2048;

typedef struct
{
	unsigned char data[16];
	int valid;
} key128;

typedef struct
{
	key128 commonkey;
	key128 ncchctrkey;
	rsakey2048 ncsdrsakey;
} keyset;

void keyset_init(keyset* keys);
int keyset_load(keyset* keys, const char* fname, int verbose);

#ifdef __cplusplus
}
#endif


#endif // _KEYSET_H_
