#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "tmd.h"
#include "utils.h"

void tmd_print(const u8* blob, u32 size)
{
	unsigned int type = getbe32(blob);
	ctr_tmd_header_4096* header4096 = 0;
	ctr_tmd_header_2048* header2048 = 0;
	ctr_tmd_body* body = 0;
	unsigned int contentcount = 0;
	unsigned int i;

	if (type == TMD_RSA_2048_SHA256 || TMD_RSA_2048_SHA1)
	{
		header2048 = (ctr_tmd_header_2048*)blob;
		body = (ctr_tmd_body*)(blob + sizeof(ctr_tmd_header_2048));
	}
	else if (type == TMD_RSA_4096_SHA256 || type == TMD_RSA_4096_SHA1)
	{
		header4096 = (ctr_tmd_header_4096*)blob;
		body = (ctr_tmd_body*)(blob + sizeof(ctr_tmd_header_4096));
	}

	contentcount = getbe16(body->contentcount);

	fprintf(stdout, "---TMD header---\n");
	fprintf(stdout, "Issuer:                 %s\n", body->issuer);
	fprintf(stdout, "Version:                %d\n", body->version);
	fprintf(stdout, "CA CRL version:         %d\n", body->ca_crl_version);
	fprintf(stdout, "Signer CRL version:     %d\n", body->signer_crl_version);
	memdump(stdout, "System version:         ", body->systemversion, 8);
	memdump(stdout, "Title id:               ", body->titleid, 8);
	fprintf(stdout, "Title type:             %08x\n", getbe32(body->titletype));
	fprintf(stdout, "Group id:               %04x\n", getbe16(body->groupid));
	fprintf(stdout, "Access rights:          %08x\n", getbe32(body->accessrights));
	fprintf(stdout, "Title version:          %04x\n", getbe16(body->titleversion));
	fprintf(stdout, "Content count:          %04x\n", getbe16(body->contentcount));
	fprintf(stdout, "Boot content:           %04x\n", getbe16(body->bootcontent));
	memdump(stdout, "Hash:                   ", body->hash, 32);
	fprintf(stdout, "---TMD content info---\n");
	for(i=0; i<64; i++)
	{
		ctr_tmd_contentinfo* info = (ctr_tmd_contentinfo*)(body->contentinfo + 36*i);

		if (getbe16(info->commandcount) == 0)
			continue;
		
		
		fprintf(stdout, "Content index:          %04x\n", getbe16(info->index));
		fprintf(stdout, "Command count:          %04x\n", getbe16(info->commandcount));
		memdump(stdout, "Unknown:                ", info->unk, 32);
	}
	fprintf(stdout, "---TMD contents---\n");
	for(i=0; i<contentcount; i++)
	{
		ctr_tmd_contentchunk* chunk = (ctr_tmd_contentchunk*)(body->contentinfo + 36*64 + i*48);
		unsigned short type = getbe16(chunk->type);

		fprintf(stdout, "Content id:             %08x\n", getbe32(chunk->id));
		fprintf(stdout, "Content index:          %04x\n", getbe16(chunk->index));
		fprintf(stdout, "Content type:           %04x", getbe16(chunk->type));
		if (type)
		{
			fprintf(stdout, " ");
			if (type & 1)
				fprintf(stdout, "[encrypted]");
			if (type & 2)
				fprintf(stdout, "[disc]");
			if (type & 4)
				fprintf(stdout, "[cfm]");
			if (type & 0x4000)
				fprintf(stdout, "[optional]");
			if (type & 0x8000)
				fprintf(stdout, "[shared]");
		}
		fprintf(stdout, "\n");
		fprintf(stdout, "Content size:           %016llx\n", getbe64(chunk->size));
		memdump(stdout, "Content hash:           ", chunk->hash, 32);
	}
}
