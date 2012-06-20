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
		return KEY_ERR_INVALID_NODE;

	const char* text = elem->GetText();
	unsigned int textlen = strlen(text);

	int status = keyset_parse_key(text, textlen, key, size, valid);

	if (status == KEY_ERR_LEN_MISMATCH)
	{
		fprintf(stderr, "Error size mismatch for key \"%s/%s\"\n", elem->Parent()->Value(), elem->Value());
		return status;
	}
	
	return status;
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
	key->valid = 0;
	if (!keyset_load_key(node.FirstChild("N"), key->n, sizeof(key->n), 0))
		goto clean;
	if (!keyset_load_key(node.FirstChild("E"), key->e, sizeof(key->e), 0))
		goto clean;
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

	key->valid = 1;
	return 1;
clean:
	return 0;
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
//	keyset_load_rsakey2048(root.FirstChild("nccholdrsakey"), &keys->nccholdrsakey);
//	keyset_load_rsakey2048(root.FirstChild("ncchdlprsakey"), &keys->ncchdlprsakey);
//	keyset_load_rsakey2048(root.FirstChild("dlpoldrsakey"), &keys->dlpoldrsakey);
//	keyset_load_rsakey2048(root.FirstChild("crrrsakey"), &keys->crrrsakey);
	keyset_load_key128(root.FirstChild("commonkey"), &keys->commonkey);
	keyset_load_key128(root.FirstChild("ncchctrkey"), &keys->ncchctrkey);


	return 1;
}


void keyset_merge(keyset* keys, keyset* src)
{
	if (src->ncchctrkey.valid)
		keyset_set_key128(&keys->ncchctrkey, src->ncchctrkey.data);
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

void keyset_set_ncchctrkey(keyset* keys, unsigned char* keydata)
{
	keyset_set_key128(&keys->ncchctrkey, keydata);
}

void keyset_parse_ncchctrkey(keyset* keys, char* keytext, int keylen)
{
	keyset_parse_key128(&keys->ncchctrkey, keytext, keylen);
}


void keyset_dump(keyset* keys)
{
	fprintf(stdout, "Current keyset:          \n");
	memdump(stdout, keys->ncchctrkey.valid? "(default) ncchctr key:  ":"(loaded) ncchctr key:  ", keys->ncchctrkey.data, 16);
	memdump(stdout, keys->commonkey.valid?  "(default) common key:   ":"(loaded) common key:   ", keys->commonkey.data, 16);
	fprintf(stdout, "\n");
}