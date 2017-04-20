#include <stdio.h>
#include "keyset.h"
#include "utils.h"
#include "tinyxml/tinyxml.h"

static void keyset_set_key128(key128* key, unsigned char* keydata);
static void keyset_parse_key128(key128* key, char* keytext, int keylen);
static int keyset_parse_key(const char* text, unsigned int textlen, unsigned char* key, unsigned int size, int* valid);
static int keyset_load_rsakey2048(TiXmlElement* elem, rsakey2048* key);
static int keyset_load_key128(TiXmlHandle node, key128* key);
static int keyset_load_key(TiXmlHandle node, unsigned char* key, unsigned int maxsize, int* valid);

static int ishex(char c)
{
	if (c >= '0' && c <= '9')
		return 1;
	if (c >= 'A' && c <= 'F')
		return 1;
	if (c >= 'a' && c <= 'f')
		return 1;
	return 0;

}

static unsigned char hextobin(char c)
{
	if (c >= '0' && c <= '9')
		return c-'0';
	if (c >= 'A' && c <= 'F')
		return c-'A'+0xA;
	if (c >= 'a' && c <= 'f')
		return c-'a'+0xA;
	return 0;
}

void keyset_init(keyset* keys)
{
	memset(keys, 0, sizeof(keyset));
}

int keyset_load_key(TiXmlHandle node, unsigned char* key, unsigned int size, int* valid)
{
	TiXmlElement* elem = node.ToElement();

	if (valid)
		*valid = 0;

	if (!elem)
		return 0;

	const char* text = elem->GetText();
	unsigned int textlen = strlen(text);

	int status = keyset_parse_key(text, textlen, key, size, valid);

	if (status == KEY_ERR_LEN_MISMATCH)
	{
		fprintf(stderr, "Error size mismatch for key \"%s/%s\"\n", elem->Parent()->Value(), elem->Value());
		return 0;
	}
	
	return 1;
}


int keyset_parse_key(const char* text, unsigned int textlen, unsigned char* key, unsigned int size, int* valid)
{
	unsigned int i, j;
	unsigned int hexcount = 0;


	if (valid)
		*valid = 0;

	for(i=0; i<textlen; i++)
	{
		if (ishex(text[i]))
			hexcount++;
	}

	if (hexcount != size*2)
	{
		fprintf(stdout, "Error, expected %d hex characters when parsing text \"", size*2);
		for(i=0; i<textlen; i++)
			fprintf(stdout, "%c", text[i]);
		fprintf(stdout, "\"\n");
		
		return KEY_ERR_LEN_MISMATCH;
	}

	for(i=0, j=0; i<textlen; i++)
	{
		if (ishex(text[i]))
		{
			if ( (j&1) == 0 )
				key[j/2] = hextobin(text[i])<<4;
			else
				key[j/2] |= hextobin(text[i]);
			j++;
		}
	}

	if (valid)
		*valid = 1;
	
	return KEY_OK;
}

int keyset_load_key128(TiXmlHandle node, key128* key)
{
	return keyset_load_key(node, key->data, sizeof(key->data), &key->valid);
}

int keyset_load_rsakey2048(TiXmlHandle node, rsakey2048* key)
{
	key->keytype = RSAKEY_INVALID;

	if (!keyset_load_key(node.FirstChild("N"), key->n, sizeof(key->n), 0))
		goto clean;
	if (!keyset_load_key(node.FirstChild("E"), key->e, sizeof(key->e), 0))
		goto clean;
	key->keytype = RSAKEY_PUB;

	if (!keyset_load_key(node.FirstChild("D"), key->d, sizeof(key->d), 0))
		goto clean;
	if (!keyset_load_key(node.FirstChild("P"), key->p, sizeof(key->p), 0))
		goto clean;
	if (!keyset_load_key(node.FirstChild("Q"), key->q, sizeof(key->q), 0))
		goto clean;
	if (!keyset_load_key(node.FirstChild("DP"), key->dp, sizeof(key->dp), 0))
		goto clean;
	if (!keyset_load_key(node.FirstChild("DQ"), key->dq, sizeof(key->dq), 0))
		goto clean;
	if (!keyset_load_key(node.FirstChild("QP"), key->qp, sizeof(key->qp), 0))
		goto clean;

	key->keytype = RSAKEY_PRIV;
clean:
	return (key->keytype != RSAKEY_INVALID);
}

int keyset_load(keyset* keys, const char* fname, int verbose)
{
	TiXmlDocument doc(fname);
	bool loadOkay = doc.LoadFile();

	if (!loadOkay)
	{
		if (verbose)
			fprintf(stderr, "Could not load keyset file \"%s\", error: %s.\n", fname, doc.ErrorDesc() );

		return 0;
	}

	TiXmlHandle root = doc.FirstChild("document");

	keyset_load_rsakey2048(root.FirstChild("ncsdrsakey"), &keys->ncsdrsakey);
	keyset_load_rsakey2048(root.FirstChild("ncchrsakey"), &keys->ncchrsakey);
	keyset_load_rsakey2048(root.FirstChild("ncchdescrsakey"), &keys->ncchdescrsakey);
	keyset_load_rsakey2048(root.FirstChild("firmrsakey"), &keys->firmrsakey);
	keyset_load_key128(root.FirstChild("commonkey"), &keys->commonkey);
	keyset_load_key128(root.FirstChild("ncchkey"), &keys->ncchkey);
	keyset_load_key128(root.FirstChild("ncchfixedsystemkey"), &keys->ncchfixedsystemkey);


	return 1;
}


void keyset_merge(keyset* keys, keyset* src)
{
	if (src->ncchkey.valid)
		keyset_set_key128(&keys->ncchkey, src->ncchkey.data);
	if (src->ncchfixedsystemkey.valid)
		keyset_set_key128(&keys->ncchfixedsystemkey, src->ncchfixedsystemkey.data);
	if (src->commonkey.valid)
		keyset_set_key128(&keys->commonkey, src->commonkey.data);
}

void keyset_set_key128(key128* key, unsigned char* keydata)
{
	memcpy(key->data, keydata, 16);
	key->valid = 1;
}

void keyset_parse_key128(key128* key, char* keytext, int keylen)
{
	keyset_parse_key(keytext, keylen, key->data, 16, &key->valid);
}

void keyset_set_commonkey(keyset* keys, unsigned char* keydata)
{
	keyset_set_key128(&keys->commonkey, keydata);
}

void keyset_parse_commonkey(keyset* keys, char* keytext, int keylen)
{
	keyset_parse_key128(&keys->commonkey, keytext, keylen);
}

void keyset_set_ncchkey(keyset* keys, unsigned char* keydata)
{
	keyset_set_key128(&keys->ncchkey, keydata);
}

void keyset_parse_ncchkey(keyset* keys, char* keytext, int keylen)
{
	keyset_parse_key128(&keys->ncchkey, keytext, keylen);
}

void keyset_set_ncchfixedsystemkey(keyset* keys, unsigned char* keydata)
{
	keyset_set_key128(&keys->ncchfixedsystemkey, keydata);
}

void keyset_parse_ncchfixedsystemkey(keyset* keys, char* keytext, int keylen)
{
	keyset_parse_key128(&keys->ncchfixedsystemkey, keytext, keylen);
}

void keyset_dump_rsakey(rsakey2048* key, const char* keytitle)
{
	if (key->keytype == RSAKEY_INVALID)
		return;


	fprintf(stdout, "%s\n", keytitle);

	memdump(stdout, "Modulus: ", key->n, 256);
	memdump(stdout, "Exponent: ", key->e, 3);

	if (key->keytype == RSAKEY_PRIV)
	{
		memdump(stdout, "P: ", key->p, 128);
		memdump(stdout, "Q: ", key->q, 128);
	}
	fprintf(stdout, "\n");
}

void keyset_dump_key128(key128* key, const char* keytitle)
{
	if (key->valid)
	{
		fprintf(stdout, "%s\n", keytitle);
		memdump(stdout, "", key->data, 16);
		fprintf(stdout, "\n");
	}
}

void keyset_dump(keyset* keys)
{
	fprintf(stdout, "Current keyset:          \n");
	keyset_dump_key128(&keys->ncchkey, "NCCH KEY");
	keyset_dump_key128(&keys->ncchfixedsystemkey, "NCCH FIXEDSYSTEMKEY");
	keyset_dump_key128(&keys->commonkey, "COMMON KEY");

	keyset_dump_rsakey(&keys->ncsdrsakey, "NCSD RSA KEY");
	keyset_dump_rsakey(&keys->ncchdescrsakey, "NCCH DESC RSA KEY");

	fprintf(stdout, "\n");
}

