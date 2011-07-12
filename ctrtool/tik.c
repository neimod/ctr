#include <stdio.h>

#include "tik.h"
#include "utils.h"

void tik_print(const u8 *blob, u32 size) {
	eticket *tik = (eticket*)blob;

	fprintf(stdout,
		"Signature Type:         %08x\n"
		"Issuer:                 %s\n",
		tik->sig_type, tik->issuer
	);

	fprintf(stdout, "Signature:\n");
	hexdump(tik->signature, 0x100);
	fprintf(stdout, "\n");

	memdump(stdout, "Encrypted Titlekey:     ", tik->encrypted_title_key, 0x10);
	memdump(stdout,	"Ticket ID:              ", tik->ticket_id, 0x08);
	memdump(stdout,	"Title ID:               ", tik->title_id, 0x08);
}
