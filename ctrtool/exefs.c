#include <stdio.h>
#include <string.h>

#include "types.h"
#include "exefs.h"
#include "utils.h"

void exefs_print(const u8 *blob, u32 size)
{
	u32 i;
	char sectname[9];
	u32 sectoffset;
	u32 sectsize;


	for(i=0; i<8; i++)
	{
		exefs_sectionheader* section = (exefs_sectionheader*)(blob + sizeof(exefs_sectionheader)*i);


		memset(sectname, 0, sizeof(sectname));
		memcpy(sectname, section->name, 8);

		sectoffset = getle32(section->offset);
		sectsize = getle32(section->size);

		if (sectsize)
		{
			fprintf(stdout, "Section name:           %s\n", sectname);
			fprintf(stdout, "Section offset:         0x%08x\n", sectoffset + 0x200);
			fprintf(stdout, "Section size:           0x%08x\n", sectsize);
		}
	}
}
