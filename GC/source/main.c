/*
 * USB-Load - Homebrew DOL loading code for the GameCube via USB-Gecko
 * - This is messy slapped together code that was untouched once it 
 * was determined that it "worked good enough code", please bare with it.
 *
 * - emu_kidid
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <stdio.h>
#include <gccore.h>		/*** Wrapper to include common libogc headers ***/
#include <ogcsys.h>		/*** Needed for console support ***/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <ogc/usbgecko.h>
#include "sidestep.h"

/*** 2D Video Globals ***/
GXRModeObj *vmode;		/*** Graphics Mode Object ***/
u32 *xfb[2] = { NULL, NULL };    /*** Framebuffers ***/
int whichfb = 0;        /*** Frame buffer toggle ***/
void ProperScanPADS(){
	PAD_ScanPads();
}

volatile unsigned long* dvd = (volatile unsigned long*)0xCC006000;

void dvd_motor_off()
{
	dvd[0] = 0x2E;
	dvd[1] = 0;

	dvd[2] = 0xe3000000;
	dvd[3] = 0;
	dvd[4] = 0;
	dvd[5] = 0;
	dvd[6] = 0;
	dvd[7] = 1; // enable reading!
	while (dvd[7] & 1);
}

/****************************************************************************
* Initialise Video
*
* Before doing anything in libogc, it's recommended to configure a video
* output.
****************************************************************************/
static void Initialise (void)
{
  VIDEO_Init ();        /*** ALWAYS CALL FIRST IN ANY LIBOGC PROJECT!
                     Not only does it initialise the video 
                     subsystem, but also sets up the ogc os
                ***/
 
  PAD_Init ();            /*** Initialise pads for input ***/
 
    /*** Try to match the current video display mode
         using the higher resolution interlaced.
    
         So NTSC/MPAL gives a display area of 640x480
         PAL display area is 640x528
    ***/
  switch (VIDEO_GetCurrentTvMode ())
    {
    case VI_NTSC:
      vmode = &TVNtsc480IntDf;
      break;
 
    case VI_PAL:
      vmode = &TVPal528IntDf;
      break;
 
    case VI_MPAL:
      vmode = &TVMpal480IntDf;
      break;
 
    default:
      vmode = &TVNtsc480IntDf;
      break;
    }
    /*** Let libogc configure the mode ***/
  VIDEO_Configure (vmode);
 
    /*** Now configure the framebuffer. 
         Really a framebuffer is just a chunk of memory
         to hold the display line by line.
    ***/
 
  xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
    /*** I prefer also to have a second buffer for double-buffering.
         This is not needed for the console demo.
    ***/
  xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
     /*** Define a console ***/
    		/*			x	y     w   h			*/
  console_init (xfb[0], 50, 180, vmode->fbWidth,480, vmode->fbWidth * 2);
    /*** Clear framebuffer to black ***/
  VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);
  VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);
 
    /*** Set the framebuffer to be displayed at next VBlank ***/
  VIDEO_SetNextFramebuffer (xfb[0]);
 
    /*** Get the PAD status updated by libogc ***/
  VIDEO_SetPostRetraceCallback (ProperScanPADS);
  VIDEO_SetBlack (0);
 
    /*** Update the video for next vblank ***/
  VIDEO_Flush ();
 
  VIDEO_WaitVSync ();        /*** Wait for VBL ***/
  if (vmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync ();
}

#define GECKO_CHANNEL 1
#define PC_READY 0x80
#define PC_OK    0x81
#define GC_READY 0x88
#define GC_OK    0x89

unsigned int convert_int(unsigned int in)
{
  unsigned int out;
  char *p_in = (char *) &in;
  char *p_out = (char *) &out;
  p_out[0] = p_in[3];
  p_out[1] = p_in[2];
  p_out[2] = p_in[1];
  p_out[3] = p_in[0];  
  return out;
}
#define __stringify(rn) #rn
#define mfspr(rn) ({unsigned int rval;  asm volatile("mfspr %0," __stringify(rn) : "=r" (rval)); rval;})

/****************************************************************************
* Main
****************************************************************************/
int main ()
{
	dvd_motor_off();
	Initialise();
	AR_Init(NULL, 0); /*** No stack - we need it all ***/
  
	*(volatile unsigned long*)0xcc00643c = 0x00000000; //allow 32mhz exi bus
	ipl_set_config(6); 

	int mram_size = (SYS_GetArenaHi()-SYS_GetArenaLo());
	int aram_size = (AR_GetSize()-AR_GetBaseAddress());
	unsigned char data = 0;
	unsigned int size = 0;
	
	printf("\n\nUSB-Gecko DOL loader for GameCube by emu_kidid\n\n");
	printf("Memory Available: [MRAM] %i KB [ARAM] %i KB\n",(mram_size/1024), (aram_size/1024));
	printf("Largest DOL possible: %i KB\n", mram_size < aram_size ? mram_size/1024:aram_size/1024);

	usb_flush(GECKO_CHANNEL);
	
	printf("\nSending ready\n");
	data = GC_READY;
	usb_sendbuffer_safe(GECKO_CHANNEL,&data,1);

	printf("Waiting for connection via USB-Gecko in Slot B ...\n");
	while((data != PC_READY) && (data != PC_OK)) {
		usb_recvbuffer_safe(GECKO_CHANNEL,&data,1);
	}
	
	if(data == PC_READY)
	{
		printf("Respond with OK\n");
		// Sometimes the PC can fail to receive the byte, this helps
		usleep(100000);
		data = GC_OK;
		usb_sendbuffer_safe(GECKO_CHANNEL,&data,1);
	}
	
	printf("Getting DOL info...\n");
	usb_recvbuffer_safe(GECKO_CHANNEL,&size,4);
	size = convert_int(size);
	printf("Size: %i bytes\n",size);
	printf("Receiving file...\n");
	unsigned char* buffer = (unsigned char*)memalign(32,size);
	unsigned char* pointer = buffer;
	
	if(!buffer) {
		printf("Failed to allocate memory!!\n");
		while(1);
	}
	
	while(size>0xF7D8)
	{
		usb_recvbuffer_safe(GECKO_CHANNEL,(void*)pointer,0xF7D8);
		size-=0xF7D8;
		pointer+=0xF7D8;
	}
	if(size)
	{
		usb_recvbuffer_safe(GECKO_CHANNEL,(void*)pointer,size);
	}
	
	DOLHEADER *dolhdr = (DOLHEADER*)buffer;
	
	printf("DOL Load address: %08X\n", dolhdr->textAddress[0]);
	printf("DOL Entrypoint: %08X\n", dolhdr->entryPoint);
	printf("BSS: %08X Size: %iKB\n", dolhdr->bssAddress, (int)((float)dolhdr->bssLength/1024));
	sleep(1);
	
	DOLtoARAM(buffer);
	while(1);
	return 0;
}
