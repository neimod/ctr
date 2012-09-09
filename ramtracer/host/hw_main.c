/*
 * hw_main.h -  Main functionality for talking to the 3DS RAM
 *               tracing/injecting hardware over USB.
 *
 * Copyright (C) 2009 Micah Dowty
 * Copyright (C) 2012 neimod
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "hw_main.h"
#include "hw_capture.h"
#include "hw_patch.h"
#include "hw_config.h"
#include "fpgaconfig.h"
#include "utils.h"
#include "elf.h"


#ifdef _WIN32
	#define PACKETS_PER_TRANSFER 8
	#define NUM_TRANSFERS 8
#else
//	#define PACKETS_PER_TRANSFER 32
//	#define NUM_TRANSFERS 1024
	#define PACKETS_PER_TRANSFER 64
	#define NUM_TRANSFERS 512
#endif



#define PATCHTAG_WRITETRIGGER		0xB00B0000
#define PATCHTAG_BYPASSTRIGGER	0xB00B0001
#define PATCHTAG_ADDPATCH			0xB00B0002
#define PATCHTAG_PROCESSENABLE	0xB00B0003


/*
 * Private functions
 */
static int HW_ReadCallback(FTDIDevice* dev, FTDICallbackType cbtype, uint8_t *buffer, int length, FTDIProgressInfo *progress, void *userdata);
static void HW_SigintHandler(int signum);


static FILE* outputFile;
static bool exitRequested;
static bool processEnabled = 0;
static HWCapture capture;
static HWPatchContext patchctx;

void HW_Init()
{
	HW_PatchInit(&patchctx);
}

/*
 * HW_Setup --
 *
 *    One-time initialization for the hardware.
 *    'bitstream' is optional. If non-NULL, the FPGA is reconfigured.
 */

void HW_Setup(FTDIDevice *dev, const char *bitstream)
{
	int err;

	if (bitstream) {
		err = FPGAConfig_LoadFile(dev, bitstream);
		if (err)
			exit(1);
	}

	err = FTDIDevice_SetMode(dev, FTDI_INTERFACE_A, FTDI_BITMODE_SYNC_FIFO, 0xFF, 0);
	if (err)
	{
		fprintf(stderr, "Error setting FTDI synchronous fifo mode\n");
		exit(1);
	}
	
	if (dev->patchcapability && patchctx.mode == 1)
	{
		HW_PatchDevice(&patchctx, dev);
	}
}

void HW_LoadFlatPatchFile(unsigned int address, const char* filename)
{
	FILE *f = fopen(filename, "rb");
	unsigned int buffersize;
	unsigned char* buffer = 0;
	
	printf("Loading patch file %s...\n", filename);
	
	if (!f) {
		perror(filename);
		exit(1);
	}
	
	fseek(f, 0, SEEK_END);
	buffersize = ftell(f);
	fseek(f, 0, SEEK_SET);
	
	buffer = malloc(buffersize);
	
	if (buffer == 0)
	{
		perror("Could not allocate buffer");
		exit(1);
	}
	
	if (fread(buffer, buffersize, 1, f) != 1) {
		perror("Error reading patchfile");
		exit(1);
	}
	
	printf("PATCH: Adding patch @ 0x%08X with size %d bytes.\n", address, buffersize);
	HW_AddPatch(&patchctx, address, buffer, buffersize);
	
	free(buffer);
	fclose(f);
}
	

void HW_LoadPatchFile(const char* filename)
{
	FILE *f = fopen(filename, "rb");
	Elf32_Ehdr ehdr;
	int phOffset;
	int i;
	
	printf("Loading patch file %s...\n", filename);
	
	if (!f) {
		perror(filename);
		exit(1);
	}
	
	/*
	 * Read the ELF header
	 */
	
	if (fread(&ehdr, sizeof ehdr, 1, f) != 1) {
		perror("ELF: Error reading ELF header");
		exit(1);
	}
	
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
		ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
		ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
		ehdr.e_ident[EI_MAG3] != ELFMAG3) {
		fprintf(stderr, "ELF: Bad magic, not an ELF file?\n");
		exit(1);
	}
	
	if (ehdr.e_ident[EI_CLASS] != ELFCLASS32 ||
		ehdr.e_ident[EI_DATA] != ELFDATA2LSB) {
		fprintf(stderr, "ELF: Not a 32-bit little-endian ELF file\n");
		exit(1);
	}
	
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT) {
		fprintf(stderr, "ELF: Bad ELF version (%d)\n", ehdr.e_ident[EI_VERSION]);
		exit(1);
	}
	
	/*
	 * Read the program header
	 */
	
	phOffset = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		Elf32_Phdr phdr;
		
		fseek(f, phOffset, SEEK_SET);
		phOffset += ehdr.e_phentsize;
		if (fread(&phdr, sizeof phdr, 1, f) != 1) {
			perror("ELF: Error reading program header");
			exit(1);
		}
		
		if (phdr.p_type == PT_NOTE) {
			uint8_t *buffer;
			uint32_t buffersize;
			uint32_t bufferpos = 0;

			if (phdr.p_filesz > 0) {
				fseek(f, phdr.p_offset, SEEK_SET);
				
				buffersize = phdr.p_filesz;
				buffer = malloc(buffersize);
				
				if (fread(buffer, buffersize, 1, f) != 1) {
					perror("ELF: Error reading segment data");
					exit(1);
				}
				
				while(bufferpos < buffersize) {
					unsigned int tag = buffer_readle32(buffer, &bufferpos, buffersize);
					unsigned int triggercount;
					unsigned int triggeraddress;
					unsigned char* triggerdata;
					unsigned int patchsize;
					unsigned int patchaddress;
					unsigned char* patchdata;
					switch(tag) 
					{
                  case PATCHTAG_PROCESSENABLE:
                     processEnabled = 1;
                     HW_SetPatchFifoHook(&patchctx, 1);
                     printf("PATCH: Enabling real-time processing.\n");
                     break;                     
						case PATCHTAG_WRITETRIGGER: 
							triggeraddress = buffer_readle32(buffer, &bufferpos, buffersize) & 0x0FFFFFFF;
							triggercount = buffer_readle32(buffer, &bufferpos, buffersize);
							triggerdata = buffer_readdata(buffer, &bufferpos, buffersize, 8);
							printf("PATCH: Setting write trigger @ 0x%08X with count %d.\n", triggeraddress, triggercount);
							HW_SetPatchWriteTrigger(&patchctx, triggeraddress, triggercount, triggerdata);
							break;
						case PATCHTAG_BYPASSTRIGGER:
							triggeraddress = buffer_readle32(buffer, &bufferpos, buffersize) & 0x0FFFFFFF;
							printf("PATCH: Setting trigger bypass @ 0x%08X.\n", triggeraddress);
							HW_SetPatchTriggerBypass(&patchctx, triggeraddress);
							break;
						case PATCHTAG_ADDPATCH:
							patchaddress = buffer_readle32(buffer, &bufferpos, buffersize) & 0x0FFFFFFF;
							patchsize = buffer_readle32(buffer, &bufferpos, buffersize);
							patchdata = buffer_readdata(buffer, &bufferpos, buffersize, patchsize);
							printf("PATCH: Adding patch @ 0x%08X with size %d bytes.\n", patchaddress, patchsize);
							HW_AddPatch(&patchctx, patchaddress, patchdata, patchsize);
							break;							
						default:
							perror("Got unknown patch tag");
							exit(1);
							break;
					}
				}
			}				
		} else if (phdr.p_type == PT_LOAD) {
			// Use the physical address
			uint32_t addr = phdr.p_paddr & 0x0FFFFFFF;
			uint8_t *buffer;
			uint32_t buffersize;

			if (phdr.p_filesz > phdr.p_memsz) {
				fprintf(stderr, "ELF: Error, file size greater than memory size\n");
				exit(1);
			}
			
			// Examine read/write flags:
			switch (phdr.p_flags & (PF_R | PF_W)) {
					
				case PF_R:  // Read-only. We can patch this.
					if (phdr.p_filesz > 0) {
						fseek(f, phdr.p_offset, SEEK_SET);
						
						buffersize = phdr.p_filesz;
						buffer = malloc(buffersize);
												
						if (fread(buffer, buffersize, 1, f) != 1) {
							perror("ELF: Error reading segment data");
							exit(1);
						}
						
						printf("PATCH: Adding patch @ 0x%08X with size %d bytes.\n", addr, buffersize);
						HW_AddPatch(&patchctx, addr, buffer, buffersize);
					}
					break;
					
				case 0:     // No-access. This is a dummy segment, ignore it.
					break;
					
				default:    // Other. Not allowed- patched memory is read-only.
					fprintf(stderr, "ELF: Patched segments must be read-only or no-access.\n");
					exit(1);
					break;
			}
		}
	}
	
	fclose(f);
	
	HW_SetPatchingMode(&patchctx, 1);
}


/*
 * HW_Trace --
 *
 *    A very high-level function to trace memory activity.
 *    Writes progress to stderr. If 'filename' is non-NULL, writes
 *    the output to disk.
 */

void HW_Trace(FTDIDevice *dev, const char *filename)
{
	int err;


	// Blank line between initialization messages and live tracing
	fprintf(stderr, "\n");

	exitRequested = false;
	outputFile = 0;

	// Open file where the captured RAM tracing data will be stored to.
	if (filename) {
		outputFile = fopen(filename, "wb");
		if (!outputFile) {
			perror("Error opening output file");
			exit(1);
		}
	} 

	// Drain any junk out of the read buffer and discard it before
	// enabling memory traces.

	while (FTDIDevice_ReadByteSync(dev, FTDI_INTERFACE_A, NULL) >= 0);

	// Capture data until we're interrupted.
	signal(SIGINT, HW_SigintHandler);

	HW_CaptureBegin(&capture, outputFile, dev, processEnabled);


	err = FTDIDevice_ReadStream(dev, FTDI_INTERFACE_A, HW_ReadCallback, NULL, PACKETS_PER_TRANSFER, NUM_TRANSFERS);
	if (err < 0 && !exitRequested)
	{
		fprintf(stderr, "Error reading stream (%d)\n", err);
		exit(1);
	}

	HW_CaptureFinish(&capture);

	if (outputFile) {
		fclose(outputFile);
		outputFile = 0;
	}

	fprintf(stdout, "\nCapture ended.\n");
}


/*
 * HW_ReadCallback --
 *
 *    Callback from fastftdi, processes one contiguous block of trace
 *    data from the device. This block is sent to disk if we have a
 *    file open.
 */

static int HW_ReadCallback(FTDIDevice* dev, FTDICallbackType cbtype, uint8_t *buffer, int length, FTDIProgressInfo *progress, void *userdata)
{
	unsigned int pos = 0;
   unsigned int canExit = 0;

   
   if (exitRequested)
   {
      canExit = HW_CaptureTryStop(&capture);
   }
   else 
   {
      
      if (length) 
         HW_CaptureDataBlock(&capture, buffer, length);
      
      if (progress) 
      {
         double seconds = progress->totalTime;
         double mb = progress->current.totalBytes / (1024.0 * 1024.0);
         double mbcomp = capture.compressedsize / (1024.0 * 1024.0);
         char x[512];
         
         fprintf(stderr, "\e]2;%10.02fs [ %9.3f/%.3f MB captured/compressed ] %7.1f kB/s current\a",
                 seconds, mb, mbcomp, progress->currentRate / 1024.0);
         fflush(stderr);
         fflush(stdout);         
      }      
   }


	return (exitRequested && canExit) ? 1 : 0;
}


/*
 * HW_SigintHandler --
 *
 *    SIGINT handler, so we can gracefully exit when the user hits ctrl-C.
 */

static void HW_SigintHandler(int signum)
{
   HW_RequestExit();
}

void HW_RequestExit()
{
  exitRequested = true;
}