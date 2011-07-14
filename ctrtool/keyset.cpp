#include <stdio.h>
#include "keyset.h"
#include "utils.h"
#include "tinyxml/tinyxml.h"

static int keyset_load_rsakey2048(TiXmlElement* elem, rsakey2048* key);
static int keyset_load_key128(TiXmlHandle node, key128* key);
static int keyset_load_key(TiXmlHandle node, unsigned char* key, unsigned int maxsize, int* valid);

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
	unsigned int i;


	if (valid)
		*valid = 0;

	if (!elem)
		return 0;

	const char* text = elem->GetText();
	unsigned int textlen = strlen(text);

	if (textlen != size*2)
	{
		fprintf(stderr, "Error size mismatch for key \"%s/%s\"\n", elem->Parent()->Value(), elem->Value());
		return 0;
	}

	for(i=0; i<size; i++)
	{
		key[i] = hextobin(text[i*2+0])<<4;
		key[i] |= hextobin(text[i*2+1]);
	}

	if (valid)
		*valid = 1;
	
	return 1;
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
	keyset_load_key128(root.FirstChild("commonkey"), &keys->commonkey);

	return 1;
}
