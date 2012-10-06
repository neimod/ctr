#ifndef __CTRCLIENT_H__
#define __CTRCLIENT_H__

#define CMD_AESCTR			0x80
#define CMD_AESCONTROL		0x81
#define CMD_AESCBCDEC		0x82
#define CMD_AESCBCENC		0x83
#define CMD_AESCCMENC		0x84
#define CMD_AESCCMDEC		0x85


#define AES_FLAGS_SET_IV		1
#define AES_FLAGS_SET_KEY		2
#define AES_FLAGS_SET_YKEY		4
#define AES_FLAGS_SELECT_KEY	8
#define AES_FLAGS_SET_NONCE		16

#define MAX_CHALLENGESIZE		0x100

typedef struct
{
	int sockfd;
} ctrclient;

typedef struct
{
	unsigned char flags[4];
	unsigned char keyslot[4];
	unsigned char key[16];
	unsigned char iv[16];
} aescontrol;

typedef struct
{
	unsigned char payloadblockcount[4];
	unsigned char assocblockcount[4];
	unsigned char maclen[4];
	unsigned char mac[16];
} aesccmheader;

// Maximum size per message
#define CHUNKMAXSIZE		0x3FF000
//#define CHUNKMAXSIZE		0x10

void ctrclient_init();
void ctrclient_disconnect(ctrclient* client);
int ctrclient_connect(ctrclient* client, const char* hostname, const char* port);
int ctrclient_sendbuffer(ctrclient* client, const void* buffer, unsigned int size);
int ctrclient_recvbuffer(ctrclient* client, void* buffer, unsigned int size);
int ctrclient_sendlong(ctrclient* client, unsigned int value);
int ctrclient_aes_ctr_crypt(ctrclient* client, unsigned char* buffer, unsigned int size);
int ctrclient_aes_cbc_decrypt(ctrclient* client, unsigned char* buffer, unsigned int size);
int ctrclient_aes_cbc_encrypt(ctrclient* client, unsigned char* buffer, unsigned int size);
int ctrclient_aes_ccm_encrypt(ctrclient* client, unsigned char* buffer, unsigned int size, unsigned char mac[16]);
int ctrclient_aes_ccm_encryptex(ctrclient* client, unsigned char* payloadbuffer, unsigned int payloadsize, unsigned char* assocbuffer, unsigned int assocsize, unsigned int maclen, unsigned char mac[16]);
int ctrclient_aes_ccm_decrypt(ctrclient* client, unsigned char* buffer, unsigned int size, unsigned char mac[16]);
int ctrclient_aes_ccm_decryptex(ctrclient* client, unsigned char* payloadbuffer, unsigned int payloadsize, unsigned char* assocbuffer, unsigned int assocsize, unsigned int maclen, unsigned char mac[16]);
int ctrclient_aes_set_key(ctrclient* client, unsigned int keyslot, unsigned char key[16]);
int ctrclient_aes_set_ykey(ctrclient* client, unsigned int keyslot, unsigned char key[16]);
int ctrclient_aes_select_key(ctrclient* client, unsigned int keyslot);
int ctrclient_aes_set_iv(ctrclient* client, unsigned char iv[16]);
int ctrclient_aes_set_ctr(ctrclient* client, unsigned char ctr[16]);
int ctrclient_aes_set_nonce(ctrclient* client, unsigned char nonce[12]);

#endif // __CTRCLIENT_H__