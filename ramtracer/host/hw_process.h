/*
 * hw_process.h - Processing of RAM tracing data.
 *
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

#ifndef __HW_PROCESS_H_
#define __HW_PROCESS_H_

#include <stdio.h>
#include <pthread.h>
#include "hw_buffer.h"
#include "fastftdi.h"
/*
 * HWCapture -- Worker structure for a seperately running thread that does heavy processing
 *              on real-time captured RAM tracing data, such as compression and saving to disk. 
 */

#define MAXFILES 16

typedef struct
{
   FILE* file;
   int used;
} filenode;

typedef struct
{
   unsigned int writefifocapacity;
   HWBuffer config;   
   HWBuffer writefifo;
   HWBuffer readfifo;   
   HWBuffer databuffer;
   FTDIDevice* dev;
   int enabled;
   filenode filemap[MAXFILES];
} HWProcess;

/*
 * Public functions
 */
void HW_ProcessInit(HWProcess* process, FTDIDevice* dev, int enabled);
void HW_ProcessDestroy(HWProcess* process);
void HW_Process(HWProcess* process, unsigned char* buffer, unsigned int buffersize);
int HW_ProcessSample(HWProcess* process, uint8_t* sampledata);
int HW_ProcessBlock(HWProcess* process, uint8_t *buffer, int length);

#endif // __HW_PROCESS_H_
