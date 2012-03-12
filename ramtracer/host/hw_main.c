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

#include "hw_main.h"
#include "hw_capture.h"
#include "fpgaconfig.h"

#ifdef _WIN32
	#define PACKETS_PER_TRANSFER 8
	#define NUM_TRANSFERS 8
#else
	#define PACKETS_PER_TRANSFER 32
	#define NUM_TRANSFERS 1024
#endif

/*
 * Private functions
 */
static int HW_ReadCallback(uint8_t *buffer, int length, FTDIProgressInfo *progress, void *userdata);
static void HW_SigintHandler(int signum);


FILE* outputFile;
bool exitRequested;
HWCapture capture;


/*
 * HW_Init --
 *
 *    One-time initialization for the hardware.
 *    'bitstream' is optional. If non-NULL, the FPGA is reconfigured.
 */

void HW_Init(FTDIDevice *dev, const char *bitstream)
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

	HW_CaptureBegin(&capture, outputFile);

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

static int HW_ReadCallback(uint8_t *buffer, int length, FTDIProgressInfo *progress, void *userdata)
{
	unsigned int pos = 0;

	if (length && outputFile) 
		HW_CaptureDataBlock(&capture, buffer, length);

	if (progress) 
	{
		double seconds = ((double)clock()) / CLOCKS_PER_SEC;
		double mb = progress->current.totalBytes / (1024.0 * 1024.0);
		double mbcomp = capture.compressedsize / (1024.0 * 1024.0);

		fprintf(stderr, "%10.02fs [ %9.3f/%.3f MB captured/compressed ] %7.1f kB/s current\r",
			seconds, mb, mbcomp, progress->currentRate / 1024.0);
	}

	return exitRequested ? 1 : 0;
}


/*
 * HW_SigintHandler --
 *
 *    SIGINT handler, so we can gracefully exit when the user hits ctrl-C.
 */

static void HW_SigintHandler(int signum)
{
   exitRequested = true;
}
