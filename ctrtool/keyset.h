#ifndef _KEYSET_H_
#define _KEYSET_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	KEY_ERR_LEN_MISMATCH,
	KEY_ERR_INVALID_NODE,
	KEY_OK
} keystatus;

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
	rsakey2048 ncchrsakey;
//	rsakey2048 nccholdrsakey;
//	rsakey2048 ncchdlprsakey;
//	rsakey2048 dlpoldrsakey;
//	rsakey2048 crrrsakey;
} keyset;

void keyset_init(keyset* keys);
int keyset_load(keyset* keys, const char* fname, int verbose);
void keyset_merge(keyset* keys, keyset* src);
void keyset_set_commonkey(keyset* keys, unsigned char* keydata);
void keyset_parse_commonkey(keyset* keys, char* keytext, int keylen);
void keyset_set_ncchctrkey(keyset* keys, unsigned char* keydata);
void keyset_parse_ncchctrkey(keyset* keys, char* keytext, int keylen);
void keyset_dump(keyset* keys);

#ifdef __cplusplus
}
#endif


#endif // _KEYSET_H_
