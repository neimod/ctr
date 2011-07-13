#include <stdio.h>

#include "types.h"
#include "utils.h"
#include "cia.h"

void cia_print(u8 *blob) {
	ctr_ciaheader *header = (ctr_ciaheader*)blob;	

	fprintf(stdout, "Header size             0x%08x\n", getle32(header->headersize));
	fprintf(stdout, "Type                    %04x\n", getle16(header->type));
	fprintf(stdout, "Version                 %04x\n", getle16(header->version));
	fprintf(stdout, "Certificate chain size  0x%04x\n", getle32(header->certsize));
	fprintf(stdout, "Ticket size             0x%04x\n", getle32(header->ticketsize));
	fprintf(stdout, "TMD size                0x%04x\n", getle32(header->tmdsize));
	fprintf(stdout, "Metasize                0x%04x\n", getle32(header->metasize));
	fprintf(stdout, "Contentsize             0x%016llx\n", getle64(header->contentsize));
}
