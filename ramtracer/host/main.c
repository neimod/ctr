/*
 * Main file for 'memhost', the command-line frontend for RAM tracing and patching.
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
#include <string.h>
#include <getopt.h>
#include <stdbool.h>

#include "fastftdi.h"
#include "hw_main.h"

static void usage(const char *argv0);


static void usage(const char *argv0)
{
   fprintf(stderr,
           "Usage: %s [options...] [trace file]\n"
           "Command-line frontend for RAM tracing and patching.\n"
           "\n"
           "Options:\n"
           "  -b, --bitstream=FILE  Load an FPGA bitstream from the provided file.\n"
           "Copyright (C) 2009 Micah Dowty <micah@navi.cx>\n",
		   "Copyright (C) 2012 neimod <neimod@gmail.com>\n",
           argv0);
   exit(1);
}


int main(int argc, char **argv)
{
   const char *bitstream = NULL;
   const char *tracefile = NULL;
   FTDIDevice dev;
   int err, c;
	
   HW_Init();

   while (1) 
   {
      int option_index;
	  char* pchr;
      static struct option long_options[] = 
	  {
         {"bitstream", 1, NULL, 'b'},
         {"patch", 1, NULL, 'p'},
         {"flatpatch", 1, NULL, 'l'},		  
         {NULL},
      };

      c = getopt_long(argc, argv, "b:p:l:", long_options, &option_index);
      if (c == -1)
         break;

      switch (c) 
	  {
      case 'b':
         bitstream = strdup(optarg);
         break;
	  case 'p':
		 HW_LoadPatchFile(optarg);
		 break;
	  case 'l':
		  {
			  char* patchfilename = 0;
			  unsigned int patchaddress = strtoul(optarg, &patchfilename, 0);
			  
			  if (patchfilename != optarg)
				  patchfilename++;
			  
 			  HW_LoadFlatPatchFile(patchaddress, patchfilename);
		  }
		 break;
      default:
         usage(argv[0]);
      }
   }

   if (optind == argc - 1)
   {
      // Exactly one extra argument- a trace file
      tracefile = argv[optind];
   } 
   else if (optind < argc) 
   {
      // Too many extra args
      usage(argv[0]);
   }

   err = FTDIDevice_Open(&dev);
   if (err) 
   {
      fprintf(stderr, "USB: Error opening device\n");
      return 1;
   }

   HW_Setup(&dev, bitstream);
   HW_Trace(&dev, tracefile);

   FTDIDevice_Close(&dev);

   return 0;
}

