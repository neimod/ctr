/*
 * fastftdi.c - A minimal FTDI FT232H interface for which supports bit-bang
 *              mode, but focuses on very high-performance support for
 *              synchronous FIFO mode. Requires libusb-1.0
 *
 * Copyright (C) 2009 Micah Dowty
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

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include "fastftdi.h"
#include "utils.h"

#define POOLTRANSFERCOUNT 32


typedef struct {
   FTDIDevice* dev;
   FTDIStreamCallback *callback;
   void *userdata;
   int result;
   FTDIProgressInfo progress;
} FTDIStreamState;


static FTDITransfer* PoolPopFree(FTDITransferPool* pool)
{
   FTDITransfer* transfer = pool->free;
   
   if (transfer)
   {
      pool->free = transfer->next;
      transfer->next = 0;
   }
   
   return transfer;
}

static void PoolPushFree(FTDITransferPool* pool, FTDITransfer* transfer)
{
   transfer->next = pool->free;
   pool->free = transfer;
}

static void PoolInit(FTDITransferPool* pool, unsigned int transfercount)
{
   unsigned int i;
   

   pool->transfer = malloc(sizeof(FTDITransfer) * transfercount);
   pool->transfercount = transfercount;
   pool->free = 0;
   
   for(i=0; i<transfercount; i++)
   {
      FTDITransfer* ftditransfer = &pool->transfer[i];

      ftditransfer->transfer = libusb_alloc_transfer(0);
      ftditransfer->next = pool->free;
      pool->free = ftditransfer;
   }
}

static FTDITransfer* DeviceTransferPop(FTDIDevice* dev)
{
   FTDITransfer* transfer = 0;
   
   
   pthread_mutex_lock(&dev->mutex);
   transfer = PoolPopFree(&dev->transferpool);
   pthread_mutex_unlock(&dev->mutex);   

   transfer->userdata = (void*)dev;

   return transfer;
}

static void DeviceTransferPush(FTDIDevice* dev, FTDITransfer* transfer)
{
   pthread_mutex_lock(&dev->mutex);
   PoolPushFree(&dev->transferpool, transfer);
   pthread_mutex_unlock(&dev->mutex);   
}

static int DeviceInit(FTDIDevice *dev)
{
   int err, interface;
   
   for (interface = 0; interface < 2; interface++) {
      if (libusb_kernel_driver_active(dev->handle, interface) == 1) {
         if ((err = libusb_detach_kernel_driver(dev->handle, interface))) {
            perror("Error detaching kernel driver");
            return err;
         }
      }
   }
   
   if ((err = libusb_set_configuration(dev->handle, 1))) {
      perror("Error setting configuration");
      return err;
   }
   
   for (interface = 0; interface < 2; interface++) {
      if ((err = libusb_claim_interface(dev->handle, interface))) {
         perror("Error claiming interface");
         return err;
      }
   }
   
   pthread_mutex_init(&dev->mutex, 0);
   PoolInit(&dev->transferpool, POOLTRANSFERCOUNT);

   return 0;
}


int FTDIDevice_Open(FTDIDevice *dev)
{
  int err;

  memset(dev, 0, sizeof *dev);

  if ((err = libusb_init(&dev->libusb))) {
    return err;
  }

  //libusb_set_debug(dev->libusb, 2);
  dev->datamask = 0x01234567;
  dev->CSI_BIT = (1<<0);
  dev->RDWR_BIT = (1<<1);
  dev->DONE_BIT = (1<<2);
  dev->PROG_BIT = (1<<3);	
  dev->devicemask = "3s500epq208";
  dev->patchcapability = 0;
  dev->handle = libusb_open_device_with_vid_pid(dev->libusb, TWLFPGA_VENDOR, TWLFPGA_PRODUCT);

  if (!dev->handle)
	dev->handle = libusb_open_device_with_vid_pid(dev->libusb, FTDI_VENDOR, FTDI_PRODUCT_FT2232H);
 
  if (!dev->handle) 
    dev->handle = libusb_open_device_with_vid_pid(dev->libusb, TEST_VENDOR, TEST_PRODUCT);
  
  if (!dev->handle) {
    dev->handle = libusb_open_device_with_vid_pid(dev->libusb, CTRFPGA_VENDOR, CTRFPGA_PRODUCT);
	dev->patchcapability = 1;
  }

  if (!dev->handle) {
    dev->handle = libusb_open_device_with_vid_pid(dev->libusb, CTRFPGA2_VENDOR, CTRFPGA2_PRODUCT);
	dev->datamask = 0x45601237;
	dev->CSI_BIT = (1<<0);
	dev->RDWR_BIT = (1<<3);
	dev->DONE_BIT = (1<<2);
	dev->PROG_BIT = (1<<1);
	dev->devicemask = "3s400afg400";
	dev->patchcapability = 1;
  }
	

  if (!dev->handle)
    return LIBUSB_ERROR_NO_DEVICE;
 

  return DeviceInit(dev);
}


void FTDIDevice_Close(FTDIDevice *dev)
{
	pthread_mutex_destroy(&dev->mutex);   
   libusb_close(dev->handle);
   libusb_exit(dev->libusb);
}


int FTDIDevice_Reset(FTDIDevice *dev)
{
  int err;

  err = libusb_reset_device(dev->handle);
  if (err)
    return err;

  return DeviceInit(dev);
}


int FTDIDevice_SetMode(FTDIDevice *dev, FTDIInterface interface,
                   FTDIBitmode mode, uint8_t pinDirections,
                   int baudRate)
{
  int err;

  err = libusb_control_transfer(dev->handle,
                                LIBUSB_REQUEST_TYPE_VENDOR
                                | LIBUSB_RECIPIENT_DEVICE
                                | LIBUSB_ENDPOINT_OUT,
                                FTDI_SET_BITMODE_REQUEST,
                                pinDirections | (mode << 8),
                                interface,
                                NULL, 0,
                                FTDI_COMMAND_TIMEOUT);
  if (err)
    return err;

  if (baudRate) {
    int divisor;

    if (mode == FTDI_BITMODE_BITBANG)
      baudRate <<= 2;

    divisor = 240000000 / baudRate;
    if (divisor < 1 || divisor > 0xFFFF) {
      return LIBUSB_ERROR_INVALID_PARAM;
    }

    err = libusb_control_transfer(dev->handle,
                                  LIBUSB_REQUEST_TYPE_VENDOR
                                  | LIBUSB_RECIPIENT_DEVICE
                                  | LIBUSB_ENDPOINT_OUT,
                                  FTDI_SET_BAUD_REQUEST,
                                  divisor,
                                  interface,
                                  NULL, 0,
                                  FTDI_COMMAND_TIMEOUT);
    if (err)
      return err;
  }

  return 0;
}


/*
 * Internal callback for cleaning up async writes.
 */
static struct libusb_transfer* globtransfer = 0;
static unsigned int globactive = 0;

static void LIBUSB_CALL WriteAsyncCallback(struct libusb_transfer *transfer)
{
   FTDITransfer* ftditransfer = (FTDITransfer*)transfer->user_data;


   free(transfer->buffer);
   
   if (ftditransfer)
   {
      FTDIDevice* dev = (FTDIDevice*)ftditransfer->userdata;

      DeviceTransferPush(dev, ftditransfer);
   }
}


/*
 * Write to an FTDI interface, either synchronously or asynchronously.
 * Async writes have no completion callback, they finish 'eventually'.
 */

int FTDIDevice_Write(FTDIDevice *dev, FTDIInterface interface, uint8_t *data, unsigned int length, bool async)
{
   int err;

   if (async) {
      FTDITransfer* ftditransfer = DeviceTransferPop(dev);
      
      
      if (!ftditransfer) {
         return LIBUSB_ERROR_BUSY;
      }

      libusb_fill_bulk_transfer(ftditransfer->transfer, dev->handle, FTDI_EP_OUT(interface), malloc(length), length, WriteAsyncCallback, ftditransfer, 0);

      if (!ftditransfer->transfer->buffer) {
         DeviceTransferPush(dev, ftditransfer);
         return LIBUSB_ERROR_NO_MEM;
      }

      memcpy(ftditransfer->transfer->buffer, data, length);
      
      err = libusb_submit_transfer(ftditransfer->transfer);
      if (err < 0) {
         DeviceTransferPush(dev, ftditransfer);
      }

   } else {
      int transferred;
      err = libusb_bulk_transfer(dev->handle, FTDI_EP_OUT(interface),
                                 data, length, &transferred,
                                 FTDI_COMMAND_TIMEOUT);
   }

   if (err < 0)
      return err;
   else
      return 0;
}


int FTDIDevice_WriteByteSync(FTDIDevice *dev, FTDIInterface interface, uint8_t byte)
{
   return FTDIDevice_Write(dev, interface, &byte, sizeof byte, false);
}


int FTDIDevice_ReadByteSync(FTDIDevice *dev, FTDIInterface interface, uint8_t *byte)
{
  /*
   * This is a simplified synchronous read, intended for bit-banging mode.
   * Ignores the modem/buffer status bytes, returns just the data.
   *
   * To make sure we have fresh data (and not the last state that was
   * hanging out in the chip's read buffer) we'll read many packets
   * and discard all but the last read.
   */

  uint8_t packet[FTDI_PACKET_SIZE * 16];
  int transferred, err;

  err = libusb_bulk_transfer(dev->handle, FTDI_EP_IN(interface),
                             packet, sizeof packet, &transferred,
                             FTDI_COMMAND_TIMEOUT);
  if (err < 0) {
    return err;
  }
  if (transferred != sizeof packet) {
    return -1;
  }

  if (byte) {
     *byte = packet[sizeof packet - 1];
  }

  return 0;
}


/*
 * Internal callback for one transfer's worth of stream data.
 * Split it into packets and invoke the callbacks.
 */

static void LIBUSB_CALL ReadStreamCallback(struct libusb_transfer *transfer)
{
   FTDIStreamState *state = transfer->user_data;


   if (state->result == 0) {
      if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {

         int i;
         uint8_t *ptr = transfer->buffer;
         int length = transfer->actual_length;
         int numPackets = (length + FTDI_PACKET_SIZE - 1) >> FTDI_LOG_PACKET_SIZE;

         for (i = 0; i < numPackets; i++) {
            int payloadLen;
            int packetLen = length;

            if (packetLen > FTDI_PACKET_SIZE)
               packetLen = FTDI_PACKET_SIZE;

            payloadLen = packetLen - FTDI_HEADER_SIZE;
            state->progress.current.totalBytes += payloadLen;

            state->result = state->callback(state->dev, FTDI_CALLBACK_DATA, ptr + FTDI_HEADER_SIZE, payloadLen,
                                            NULL, state->userdata);
            
            if (state->result)
               break;

            ptr += packetLen;
            length -= packetLen;
         }

      } else {
         state->result = LIBUSB_ERROR_IO;
      }
   }

   if (state->result == 0) {
      transfer->status = -1;
      state->result = libusb_submit_transfer(transfer);
   }
}


static double TimevalDiff(const struct timeval *a, const struct timeval *b)
{
   return (a->tv_sec - b->tv_sec) + 1e-6 * (a->tv_usec - b->tv_usec);
}


/*
 * Use asynchronous transfers in libusb-1.0 for high-performance
 * streaming of data from a device interface back to the PC. This
 * function continuously transfers data until either an error occurs
 * or the callback returns a nonzero value. This function returns
 * a libusb error code or the callback's return value.
 *
 * For every contiguous block of received data, the callback will
 * be invoked.
 */


int FTDIDevice_ReadStream(FTDIDevice *dev, FTDIInterface interface, FTDIStreamCallback *callback, void *userdata, int packetsPerTransfer, int numTransfers)
{
   struct libusb_transfer **transfers;
   FTDIStreamState state = { dev, callback, userdata };
   int bufferSize = packetsPerTransfer * FTDI_PACKET_SIZE;
   int xferIndex;
   int err = 0;
	
   
   /*
    * Set up all transfers
    */
   
   transfers = calloc(numTransfers, sizeof *transfers);
   if (!transfers) {
      err = LIBUSB_ERROR_NO_MEM;
      goto cleanup;
   }
   
   for (xferIndex = 0; xferIndex < numTransfers; xferIndex++) {
      struct libusb_transfer *transfer;
      
      transfer = libusb_alloc_transfer(0);
      transfers[xferIndex] = transfer;
      if (!transfer) {
         err = LIBUSB_ERROR_NO_MEM;
         goto cleanup;
      }
      
      libusb_fill_bulk_transfer(transfer, dev->handle, FTDI_EP_IN(interface),
                                malloc(bufferSize), bufferSize, ReadStreamCallback,
                                &state, 0);
      
      if (!transfer->buffer) {
         err = LIBUSB_ERROR_NO_MEM;
         goto cleanup;
      }
      
      transfer->status = -1;
      err = libusb_submit_transfer(transfer);
      if (err)
         goto cleanup;
   }
   
   /*
    * Run the transfers, and periodically assess progress.
    */
   
   gettimeofday(&state.progress.first.time, NULL);
   
   do {
      FTDIProgressInfo  *progress = &state.progress;
      const double progressInterval = 0.1;
      struct timeval timeout = { 0, 10000 };
      struct timeval now;
      
      int err = libusb_handle_events_timeout(dev->libusb, &timeout);
      if (!state.result) {
         state.result = err;
      }
      
      // If enough time has elapsed, update the progress
      gettimeofday(&now, NULL);
      if (TimevalDiff(&now, &progress->current.time) >= progressInterval) {
         double currentTime;
         
         
         progress->current.time = now;
         
         progress->totalTime = TimevalDiff(&progress->current.time, &progress->first.time);
         currentTime = TimevalDiff(&progress->current.time, &progress->prev.time);
         
         progress->totalRate = progress->current.totalBytes / progress->totalTime;
         progress->currentRate = (progress->current.totalBytes - progress->prev.totalBytes) / currentTime;
         
         state.result = state.callback(state.dev, FTDI_CALLBACK_PERIODICAL, NULL, 0, progress, state.userdata);
         
         
         progress->prev = progress->current;
      }	   
   } while (!state.result);
   
   while(1)
   {
      if (0 != state.callback(state.dev, FTDI_CALLBACK_CLEANUP, NULL, 0, &state.progress, state.userdata))
         break;
      
      mssleep(1);
   }
   
   printf("\n\nCleaning up\n\n");
   
   /*
    * Cancel any outstanding transfers, and free memory.
    */
   
cleanup:
   if (transfers) {
      for (xferIndex = 0; xferIndex < numTransfers; xferIndex++) {
         struct libusb_transfer *transfer = transfers[xferIndex];
         
         if (transfer) {
            if (transfer->status == -1)
               libusb_cancel_transfer(transfer);
            free(transfer->buffer);
            libusb_free_transfer(transfer);
         }
      }
      free(transfers);
   }
      
   if (err)
      return err;
   else
      return state.result;
}
