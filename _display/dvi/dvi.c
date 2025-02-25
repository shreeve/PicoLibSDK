
// ****************************************************************************
//
//                                DVI (HDMI)
//
// ****************************************************************************
// PicoLibSDK - Alternative SDK library for Raspberry Pico and RP2040
// Copyright (c) 2023 Miroslav Nemecek, Panda38@seznam.cz, hardyplotter2@gmail.com
// 	https://github.com/Panda381/PicoLibSDK
//	https://www.breatharian.eu/hw/picolibsdk/index_en.html
//	https://github.com/pajenicko/picopad
//	https://picopad.eu/en/
// License:
//	This source code is freely available for any purpose, including commercial.
//	It is possible to take and modify the code or parts of it, without restriction.

#include "../../global.h"	// globals

#if USE_DVI					// use DVI (HDMI) display with simple frame buffer:
						//	1=use only frame buffer
						//	2=add full back buffer
						//	3=add 1/2 back buffer
						//	4=add 1/4 back buffer
						//	5=add 1/8 back buffer

#include "dvi.h"
#include "../../_sdk/inc/sdk_dma.h"
#include "../../_sdk/inc/sdk_pwm.h"
#include "../../_sdk/inc/sdk_cpu.h"
#include "../../_sdk/inc/sdk_pio.h"
#include "../../_sdk/inc/sdk_irq.h"
#include "../../_sdk/inc/sdk_interp.h"
#include "../../_sdk/inc/sdk_gpio.h"
#include "../../_sdk/inc/sdk_timer.h"
#include "../../_sdk/inc/sdk_multicore.h"
#include "dvi.pio.h"
#include "../minivga/minivga.h" // VGA display

#define DVI_PIO_OFF	0	// offset of VGA program in PIO memory (must be 0!)
#define DVI_LANES	3	// number of TMDS lanes (0: blue, 1: green, 2: red, 3: clock)
#define DVI_SYNC_LANE	0	// synchronization lane (= blue)
#define DVI_INTERP	0	// interpolator used to encode pixels

#define DVICLK_SLICE	PWM_GPIOTOSLICE(DVI_GPIO_CLK) // PWM slice index

#define DBUF_SIZE	(DVI_HACT/2*4)	// size of one line buffer in bytes (640/2=320 pixels, 1 pixels sent as 2 pixels in one u32 word)

#define DVI_SM1		(DVI_SM0+1) 	// DVI state machine for lane 1
#define DVI_SM2		(DVI_SM0+2) 	// DVI state machine for lane 2

#define DVI_SM(lane)	(DVI_SM0+(lane)) // DVI state machine for specified lane

#define DVI_DMA_CB0	(DVI_DMA+0)	// DVI control DMA channel for lane 0
#define DVI_DMA_DB0	(DVI_DMA+1)	// DVI data DMA channel for lane 0
#define DVI_DMA_CB1	(DVI_DMA+2)	// DVI control DMA channel for lane 1
#define DVI_DMA_DB1	(DVI_DMA+3)	// DVI data DMA channel for lane 1
#define DVI_DMA_CB2	(DVI_DMA+4)	// DVI control DMA channel for lane 2
#define DVI_DMA_DB2	(DVI_DMA+5)	// DVI data DMA channel for lane 2

#define DVI_DMA_CB(lane) (DVI_DMA+(lane)*2)	// DVI control DMA channel for specified lane
#define DVI_DMA_DB(lane) (DVI_DMA+(lane)*2+1)	// DVI data DMA channel for specified lane

// TMDS control symbols (every symbol if twice, 2 x 10 bits in one 32-bit word)
//   bit 0: HSYNC, bit 1: VSYNC ... sent to sync lane 0 (= blue)
u32 DviCtrlSyms[5] = // don't use "const" to keep the table in faster RAM
{
	// negative polarity
	0xaaeab,	// 0: (0x2AB << 10) | 0x2AB, no sync
	0x55154,	// 1: (0x154 << 10) | 0x154, HSYNC
	0x2acab,	// 2: (0x0AB << 10) | 0x0AB, VSYNC
	0xd5354,	// 3: (0x354 << 10) | 0x354, HSYNC + VSYNC

/*
	// positive polarity
	0xd5354,	// 0: (0x354 << 10) | 0x354, no sync
	0x2acab,	// 1: (0x0AB << 10) | 0x0AB, HSYNC
	0x55154,	// 2: (0x154 << 10) | 0x154, VSYNC
	0xaaeab,	// 3: (0x2AB << 10) | 0x2AB, HSYNC + VSYNC
*/

	0x7fd00,	// 4: (0x1ff << 10) | 0x100, dark line
};

// TMDS data table
u32 TmdsTable[64] = // don't use "const" to keep the table in faster RAM
{
#include "dvi_tab.h"
};

// data
volatile int DviScanLine;	// current scan line 1...
volatile u32 DviFrame;		// frame counter
volatile int DviBufInx;		// current data buffer index (0 or 1)
//volatile Bool DviVSync;		// current scan line is vsync or dark

#if DVI_IRQTIME				// debug flag - measure delta time of DVI service
volatile u32 DviTimeIn;			// time in interrupt service, in [us]
volatile u32 DviTimeOut;		// time out interrupt service, in [us]
volatile u32 DviTimeIn2;		// time in interrupt service, in [us]
volatile u32 DviTimeOut2;		// time out interrupt service, in [us]
u32 DviTimeTmp;
#endif // DVI_IRQTIME

// data buffers to decode graphics lines (= 640/2*4*3*2 = 7680 bytes)
u32 DviLineBuf[DBUF_SIZE/4*DVI_LANES*2]; // even and odd line buffer for 3 lanes

// control buffers with DMA command list
// - lane 0 control buffers contain 4 DMA command segments: front porch, HSYNC, back porch + IRQ, data
// - lane 1+2 control buffers contain 2 DMA command segments: front+hsync+back porch, data
// - every DMA command segment requires 4 u32 entries (4 registers: read, write, count and control)
// - The IRQ interrupt occurs from the data DMA channel at the end of the back porch, during data
//   segment activation. This provides a sufficient time reserve for possible IRQ service delays.
u32 DviLineBufSync[4*4];	// lane 0 vertical sync (front+VSYNC, HSYNC+VSYNC, back+VSYNC+IRQ, dark+VSYNC)
u32 DviLineBufDark0[4*4];	// lane 0 dark line (front, HSYNC, back+IRQ, dark)
u32 DviLineBufDark12[2*4 * 2];	// lane 1+2 dark line (front+hsync+back, dark)
u32 DviLineBufImg0[4*4 * 2];	// lane 0 image line, 2 buffers (even and odd line; front, HSYNC, back+IRQ, image)
u32 DviLineBufImg12[2*4 * 4];	// lane 1+2 image lines, 2 buffers (even and odd line; front+hsync+back, image)

// next control buffer
u32*	DviCtrlBufNext[DVI_LANES];

// TMDS pins
const u8 DviPins[3] = {
	DVI_GPIO_D0,
	DVI_GPIO_D1,
	DVI_GPIO_D2,
};

// encode data
void NOFLASH(DviEncode)(int line, int bufinx)
{
	// save interpolators
	sInterpSave save0, save1;
	InterpSave(0, &save0);
	InterpSave(1, &save1);

	// reset interpolators
	InterpReset(0);
	InterpReset(1);

// ==== prepare RED channel (interpolator 0)

	// prepare channel position - red
#define CHANNEL_LSB	11	// red channel least significant bit
#define CHANNEL_MSB	15	// red channel most significant bit
#define PIXEL_WIDTH	16	// pixel width in bits
#define INDEX_SHIFT	2	// convert index to offset in LUT table
#define PIXEL_LSB	0	// least significant bit of the pixel
#define LUT_INDEX_WIDTH	6	// number of bits per index in LUT table (= 64 entries)
#define INDEX_MSB	(INDEX_SHIFT + LUT_INDEX_WIDTH - 1) // most significant bit of the index

	// shift channel to offset in the LUT table, red
	//  PIXEL_LSB ... >> bit offset of the pixel in input u32, this will normalize pixel to base bit position
	//  channel_msb ... >> this will shift last bit of the channel to bit position 0
	//  +1 ... >> this will shift whole channel under bit position 0
	//  -LUT_INDEX_WIDTH ... << this will shift channel that only usable bits will be visible = index in LUT table
	//  -INDEX_SHIFT ... << this will convert pixel index to the offset in the LUT table
	//  +channel_preshift ... << this will pre-shift blue channel 3 bits left to correct negatibe position
#define SHIFT_CHANNEL_TO_INDEX (PIXEL_LSB + CHANNEL_MSB + 1 - LUT_INDEX_WIDTH - INDEX_SHIFT)

	// red: 0 + 15 + 1 - 6 - 2 = 8 (delete 5+6 bits without 2 bits)

	// setup lane 0 for 1st pixel, red
	InterpShift(0, 0, SHIFT_CHANNEL_TO_INDEX); // set shift for 1st pixel
	InterpMask(0, 0, INDEX_MSB - (CHANNEL_MSB - CHANNEL_LSB), INDEX_MSB); // mask least and most significant bit

	// setup lane 1 for 2nd pixel, red
	InterpShift(0, 1, PIXEL_WIDTH + SHIFT_CHANNEL_TO_INDEX); // set shift for 2nd pixel
	InterpMask(0, 1, INDEX_MSB - (CHANNEL_MSB - CHANNEL_LSB), INDEX_MSB); // mask least and most significant bit
	InterpCrossInput(0, 1, True); // feed lane's 0 accumulator into this lane's shift+mask input

	// setup base
	InterpBase(0, 0, (u32)TmdsTable); // set LUT table as base of lane 0
	InterpBase(0, 1, (u32)TmdsTable); // set LUT table as base of lane 1

#undef CHANNEL_LSB
#undef CHANNEL_MSB
#undef SHIFT_CHANNEL_TO_INDEX

// ==== prepare GREEN channel (interpolator 1)

	// prepare channel position - green
#define CHANNEL_LSB	5	// green channel least significant bit
#define CHANNEL_MSB	10	// green channel most significant bit

	// shift channel to offset in the LUT table, green
	//  PIXEL_LSB ... >> bit offset of the pixel in input u32, this will normalize pixel to base bit position
	//  channel_msb ... >> this will shift last bit of the channel to bit position 0
	//  +1 ... >> this will shift whole channel under bit position 0
	//  -LUT_INDEX_WIDTH ... << this will shift channel that only usable bits will be visible = index in LUT table
	//  -INDEX_SHIFT ... << this will convert pixel index to the offset in the LUT table
	//  +channel_preshift ... << this will pre-shift blue channel 3 bits left to correct negatibe position
#define SHIFT_CHANNEL_TO_INDEX (PIXEL_LSB + CHANNEL_MSB + 1 - LUT_INDEX_WIDTH - INDEX_SHIFT)

	// green: 0 + 10 + 1 - 6 - 2 = 3 (delete 5 bits without 2 bits)

	// setup lane 0 for 1st pixel, green
	InterpShift(1, 0, SHIFT_CHANNEL_TO_INDEX); // set shift for 1st pixel
	InterpMask(1, 0, INDEX_MSB - (CHANNEL_MSB - CHANNEL_LSB), INDEX_MSB); // mask least and most significant bit

	// setup lane 1 for 2nd pixel, green
	InterpShift(1, 1, PIXEL_WIDTH + SHIFT_CHANNEL_TO_INDEX); // set shift for 2nd pixel
	InterpMask(1, 1, INDEX_MSB - (CHANNEL_MSB - CHANNEL_LSB), INDEX_MSB); // mask least and most significant bit
	InterpCrossInput(1, 1, True); // feed lane's 0 accumulator into this lane's shift+mask input

	// setup base
	InterpBase(1, 0, (u32)TmdsTable); // set LUT table as base of lane 0
	InterpBase(1, 1, (u32)TmdsTable); // set LUT table as base of lane 1

// ==== convert RED and GREEN channels

	// pointer to source data line
	u16* data = &FrameBuf[line*WIDTHLEN];

	// destination buffers
	u32* dstR = &DviLineBuf[(2*2+bufinx)*DBUF_SIZE/4]; // pointer to destination buffer - red component
	u32* dstG = &DviLineBuf[(1*2+bufinx)*DBUF_SIZE/4]; // pointer to destination buffer - green component

	// decode scanline data - Red and Green components
	//  inbuf ... input buffer (u16), must be u32 aligned
	//  outbufR ... output buffer, red component (u32)
	//  outbufG ... output buffer, green component (u32)
	//  count ... number of pixels (must be multiply of 8)
	DviEncRG(data, dstR, dstG, WIDTH);

// ==== convert BLUE channel

	// destination buffer
	u32* dst = &DviLineBuf[(0*2+bufinx)*DBUF_SIZE/4]; // pointer to destination buffer - blue component

	// encode data of lane 0 (blue channel)
	DviEncB(data, dst, WIDTH);

	// restore interpolators
	InterpLoad(1, &save1);
	InterpLoad(0, &save0);
}

// DVI DMA handler - called on end of every scanline
// - The IRQ interrupt occurs from the data DMA channel at the end of the back porch, during data
//   segment activation. This provides a sufficient time reserve for possible IRQ service delays.
void NOFLASH(DviLine)()
{
#if DVI_IRQTIME				// debug flag - measure delta time of DVI service
	u32 t1 = Time();		// start time
#endif // DVI_IRQTIME

	// Clear the interrupt request for DMA control channel
	DMA_IRQ1Clear(DVI_DMA_DB0);

	// wait to fill data DMA by data segment
	int lane;
	for (lane = 0; lane < DVI_LANES; lane++)
	{
		// "NEXT count" of data DMA must be equal to "WIDTH" on last command segment
		while (DMA_Next(DVI_DMA_DB(lane)) != DVI_HACT/2) {}

		// set next control buffer
		DMA_SetRead(DVI_DMA_CB(lane), DviCtrlBufNext[lane]);
	}

	// increment scanline (1..)
	int line = DviScanLine; // current scanline
	line++; 		// new current scanline
	if (line >= DVI_VTOTAL) // last scanline?
	{
		DviFrame++;	// increment frame counter
		line = 0; 	// restart scanline

//GPIO_Flip(LED_PIN);

	}
	DviScanLine = line;	// store new scanline

	// VSYNC line
	line -= DVI_VSYNC;
	if (line < 0)
	{
		DviCtrlBufNext[0] = DviLineBufSync; // lane 0 vertical sync
		DviCtrlBufNext[1] = DviLineBufDark12; // lane 1 dark line
		DviCtrlBufNext[2] = &DviLineBufDark12[2*4]; // lane 2 dark line
	}
	else
	{
		// front porch and back porch (dark line)
		line -= DVI_VBACK;
		if ((line < 0) || (line >= DVI_VACT))
		{
			DviCtrlBufNext[0] = DviLineBufDark0; // lane 0 dark line
			DviCtrlBufNext[1] = DviLineBufDark12; // lane 1 dark line
			DviCtrlBufNext[2] = &DviLineBufDark12[2*4]; // lane 2 dark line
		}

		// image scanlines
		else
		{
			// current buffer index
			int bufinx = DviBufInx;

			// lines are duplicated, so only even lines need to be encoded
			if ((line & 1) == 0)
			{
				// encode data
				DviEncode(line/2, bufinx);
			}
			else
			{
				// switch current data buffer index (bufinx = current preparing buffer, DviBufInx = current running buffer)
				DviBufInx = bufinx ^ 1;
			}

			// set next control buffer
			DviCtrlBufNext[0] = &DviLineBufImg0[4*4 * bufinx]; // lane 0 image line
			DviCtrlBufNext[1] = &DviLineBufImg12[2*4 * bufinx]; // lane 1 image line
			DviCtrlBufNext[2] = &DviLineBufImg12[2*4 * (bufinx+2)]; // lane 2 image line


//			DviCtrlBufNext[0] = DviLineBufDark0; // lane 0 dark line
//			DviCtrlBufNext[1] = DviLineBufDark12; // lane 1 dark line
//			DviCtrlBufNext[2] = &DviLineBufDark12[2*4]; // lane 2 dark line

		}
	}

#if DVI_IRQTIME					// debug flag - measure delta time of DVI service
	if (line == 100)
	{
		u32 t2 = Time();		// stop time
		DviTimeIn = t2 - t1;		// time in interrupt service
		DviTimeTmp = t2;
	}

	if (line == 101)
	{
		DviTimeOut = t1 - DviTimeTmp;	// time out interrupt service

		u32 t2 = Time();		// stop time
		DviTimeIn2 = t2 - t1;		// time in interrupt service
		DviTimeTmp = t2;
	}

	if (line == 102)
	{
		DviTimeOut2 = t1 - DviTimeTmp;	// time out interrupt service
	}
#endif // DVI_IRQTIME
}

// configure one output pin
void DviPinInit(u8 pin)
{
	GPIO_Drive2mA(pin);	// 8mA drive (options: 2/4/8/12 mA)
	GPIO_Slow(pin);		// use slow slew rate control (options: Slow/Fast)

//	GPIO_Drive12mA(pin);	// 8mA drive (options: 2/4/8/12 mA)
//	GPIO_Fast(pin);		// use slow slew rate control (options: Slow/Fast)

	GPIO_InDisable(pin);	// input disable
	GPIO_NoPull(pin);	// no pulls
//	GPIO_PullUp(pin);	// no pulls
}

// initialize PIO of serializer
void DviPioInit()
{
	int i;
	u8 pin, sm;

	// initialize PIO
	PioInit(DVI_PIO);

	// load PIO program
	PioLoadProg(DVI_PIO, dvi_program_instructions, count_of(dvi_program_instructions), DVI_PIO_OFF);

	// initialize all lanes
	for (i = 0; i < DVI_LANES; i++)
	{
		sm = DVI_SM(i);

		// PIO set wrap address
		PioSetWrap(DVI_PIO, sm, DVI_PIO_OFF + dvi_wrap_target, DVI_PIO_OFF + dvi_wrap);

		// set start address
		PioSetAddr(DVI_PIO, sm, DVI_PIO_OFF);

		// initialize GPIOs
		pin = DviPins[i];
		PioSetupGPIO(DVI_PIO, pin, 2); // setup pins for use by PIO
		DviPinInit(pin);		// setup first pin
		PioSetPin(DVI_PIO, sm, pin, 1, 0); // set pin1 output value to 0
		DviPinInit(pin+1);		// setup second pin
		PioSetPin(DVI_PIO, sm, pin+1, 1, 1); // set pin2 output value to 1 
		PioSetPinDir(DVI_PIO, sm, pin, 2, 1); // set pin direction to output

		// set sideset pins (2 bits, no optional, no pindirs)
		PioSetupSideset(DVI_PIO, sm, pin, 2, False, False);

		// join FIFO to send only
		PioSetFifoJoin(DVI_PIO, sm, PIO_FIFO_JOIN_TX);

		// set PIO clock divider to 1.00
		PioSetClkdiv(DVI_PIO, sm, 1*256);

		// shift right, autopull, pull threshold = 20 bits (2 symbols per 32-bit word)
		PioSetOutShift(DVI_PIO, sm, True, True, 2*10);
	}
}

// initialize PWM of serializer
void DviPwmInit()
{
STATIC_ASSERT((DVI_GPIO_CLK & 1) == 0, "DVI_GPIO_CLK must be even!");

	// use PWM slice to drive pixel clock
	PWM_InvEnable(DVICLK_SLICE, 0); // invert channel A
	PWM_InvDisable(DVICLK_SLICE, 1); // non-invert channel B
	PWM_GpioInit(DVI_GPIO_CLK);	// set PWM function of first pin
	PWM_GpioInit(DVI_GPIO_CLK+1);	// set PWM function of second pin
	DviPinInit(DVI_GPIO_CLK);	// setup first pin
	DviPinInit(DVI_GPIO_CLK+1);	// setup second pin

	PWM_Top(DVICLK_SLICE, 9);	// set wrap value to 9 (period = 10)
	PWM_ClkDiv(DVICLK_SLICE, 1*16);	// set clock divider to 1.00
	PWM_Comp(DVICLK_SLICE, 0, 5);	// set compare value of channel A to 5
	PWM_Comp(DVICLK_SLICE, 1, 5);	// set compare value of channel B to 5
}

// prepare one command segment of the control buffer (returns new pointer to control buffer)
//  cb ... pointer to control buffer (fills 4 entries)
//  lane ... lane index 0..2
//  read ... read address
//  count ... number of transfers
//  ring ... order of read ring size in bytes, 0=no ring, 2=4 bytes
//  irq ... raise IRQ after finishing this command segment (use after back porch)
// - using alias 0 of DMA channel registers (four u32 registers = 16 bytes)
//	#define DMA_CH_READ		0	// DMA channel read address
//	#define DMA_CH_WRITE		1	// DMA channel write address
//	#define DMA_CH_COUNT		2	// DMA channel transfer count (auto reloaded on trigger)
//	#define DMA_CH_CTRL_TRIG	3	// DMA channel control and status + trigger
u32* DviSetCb(u32* cb, int lane, const void* read, int count, int ring, Bool irq)
{
	*cb++ = (u32)read;		// read address from data buffer
	*cb++ = (u32)PIO_TXF(DVI_PIO, DVI_SM(lane)); // write address - transfer FIFO of the PIO state machine
	*cb++ = count;			// number of transfers
	*cb++ = 			// control word
		// DMA_CTRL_SNIFF |	// not sniff
		// DMA_CTRL_BSWAP |	// byte swap
		(irq ? 0 : DMA_CTRL_QUIET) |	// quiet if not IRQ
		DMA_CTRL_TREQ(PioGetDreq(DVI_PIO, DVI_SM(lane), True)) | // data request from PIO
		DMA_CTRL_CHAIN(DVI_DMA_CB(lane)) | // chain to control DMA channel
		// DMA_CTRL_RING_WRITE | // not wrap ring on write
		DMA_CTRL_RING_SIZE(ring) | // order of ring size in bytes
		// DMA_CTRL_INC_WRITE |	// not increment write
		DMA_CTRL_INC_READ |	// increment read
		DMA_CTRL_SIZE(DMA_SIZE_32) | // data size 32 bits
		//DMA_CTRL_HIGH_PRIORITY | // high priority
		DMA_CTRL_EN;		// enable DMA

	return cb;
}

// initialize control buffers
void DviBufInit()
{
// command segments: 1) front porch, 2) HSYNC, 3) back porch+IRQ, 4) data

	u32 *cb, *db;
	int i;

	// lane 0 vertical sync
	cb = DviSetCb(DviLineBufSync, 0, &DviCtrlSyms[2], DVI_HFRONT/2, 2, False); // front porch + VSYNC
	cb = DviSetCb(cb, 0, &DviCtrlSyms[3], DVI_HSYNC/2, 2, False); // HSYNC + VSYNC
	cb = DviSetCb(cb, 0, &DviCtrlSyms[2], DVI_HBACK/2, 2, True); // back porch + VSYNC + IRQ
	DviSetCb(cb, 0, &DviCtrlSyms[2], DVI_HACT/2, 2, False); // dark + VSYNC

	// lane 0 dark line
	cb = DviSetCb(DviLineBufDark0, 0, &DviCtrlSyms[0], DVI_HFRONT/2, 2, False); // front porch
	cb = DviSetCb(cb, 0, &DviCtrlSyms[1], DVI_HSYNC/2, 2, False); // HSYNC
	cb = DviSetCb(cb, 0, &DviCtrlSyms[0], DVI_HBACK/2, 2, True); // back porch + IRQ
	DviSetCb(cb, 0, &DviCtrlSyms[4], DVI_HACT/2, 2, False); // dark

	// lane 1 dark line
	cb = DviSetCb(DviLineBufDark12, 1, &DviCtrlSyms[0], (DVI_HFRONT+DVI_HSYNC+DVI_HBACK)/2, 2, False); // front+hsync+back porch
	cb = DviSetCb(cb, 1, &DviCtrlSyms[0], DVI_HACT/2, 2, False); // dark

	// lane 2 dark line
	cb = DviSetCb(cb, 2, &DviCtrlSyms[0], (DVI_HFRONT+DVI_HSYNC+DVI_HBACK)/2, 2, False); // front+hsync+back porch
	DviSetCb(cb, 2, &DviCtrlSyms[0], DVI_HACT/2, 2, False); // dark

	// lane 0 image lines
	cb = DviLineBufImg0;
	db = DviLineBuf;
	for (i = 2; i > 0; i--)
	{
		cb = DviSetCb(cb, 0, &DviCtrlSyms[0], DVI_HFRONT/2, 2, False); // front porch
		cb = DviSetCb(cb, 0, &DviCtrlSyms[1], DVI_HSYNC/2, 2, False); // HSYNC
		cb = DviSetCb(cb, 0, &DviCtrlSyms[0], DVI_HBACK/2, 2, True); // back porch + IRQ
		cb = DviSetCb(cb, 0, db, DVI_HACT/2, 0, False); // image
		db += DBUF_SIZE/4;
	}

	// lane 1+2 image lines
	cb = DviLineBufImg12;
	for (i = 0; i < 4; i++)
	{
		cb = DviSetCb(cb, i/2+1, &DviCtrlSyms[0], (DVI_HFRONT+DVI_HSYNC+DVI_HBACK)/2, 2, False); // front+hsync+back porch
		cb = DviSetCb(cb, i/2+1, db, DVI_HACT/2, 0, False); // image
		db += DBUF_SIZE/4;
	}
}

// DVI initialize DMA
// initialize VGA DMA
//   control blocks aliases:
//                  +0x0        +0x4          +0x8          +0xC (Trigger)
// 0x00 (alias 0):  READ_ADDR   WRITE_ADDR    TRANS_COUNT   CTRL_TRIG ... we use this!
// 0x10 (alias 1):  CTRL        READ_ADDR     WRITE_ADDR    TRANS_COUNT_TRIG
// 0x20 (alias 2):  CTRL        TRANS_COUNT   READ_ADDR     WRITE_ADDR_TRIG
// 0x30 (alias 3):  CTRL        WRITE_ADDR    TRANS_COUNT   READ_ADDR_TRIG
void DviDmaInit()
{
	int lane;
	for (lane = 0; lane < DVI_LANES; lane++)
	{
		// prepare DMA control channel
		DMA_Config(DVI_DMA_CB(lane),		// channel
			(lane == 0) ? DviLineBufSync : ((lane == 1) ? DviLineBufDark12 : &DviLineBufDark12[2*4]), // read address
			&DMA_CHAN(DVI_DMA_DB(lane))[DMA_CH_READ], // write address to READ register of alias 0
			4,			// number of transfers = 4 * u32 (= 1 command segment)
				// DMA_CTRL_SNIFF |	// not sniff
				// DMA_CTRL_BSWAP |	// not byte swap
				// DMA_CTRL_QUIET |	// not quiet
				DMA_CTRL_TREQ_FORCE |	// permanent request
				DMA_CTRL_CHAIN(DVI_DMA_CB(lane)) | // disable chaining
				DMA_CTRL_RING_WRITE |	// wrap ring on write
				DMA_CTRL_RING_SIZE(4) | // ring size = 16 bytes
				DMA_CTRL_INC_WRITE |	// increment write
				DMA_CTRL_INC_READ |	// increment read
				DMA_CTRL_SIZE(DMA_SIZE_32) | // data size 32 bits
				// DMA_CTRL_HIGH_PRIORITY | // not high priority
				DMA_CTRL_EN);		// enable DMA
	}

	// enable DMA channel IRQ0
	DMA_IRQ1Enable(DVI_DMA_DB0);

	// set DMA IRQ handler
	SetHandler(IRQ_DMA_1, DviLine);

	// set highest IRQ priority
	NVIC_IRQPrio(IRQ_DMA_1, IRQ_PRIO_REALTIME);
}

// DVI output enable
void DviEnable()
{
	// interrupt disable
	IRQ_LOCK;

// clock and data do not have to be exactly
// synchronized, DVI allows some phase offset

	// enable clock PWM
	PWM_Enable(DVICLK_SLICE);

	// enable state machines
	PioSMEnableMaskSync(DVI_PIO, RangeMask(DVI_SM0, DVI_SM0+DVI_LANES-1));

	// interrupt enable
	IRQ_UNLOCK;
}

// DVI output disable
void DviDisable()
{
	// disable state machines
	PioSMDisableMask(DVI_PIO, RangeMask(DVI_SM0, DVI_SM0+DVI_LANES-1));

	// disable clock PWM
	PWM_Disable(DVICLK_SLICE);
}

// initialize DVI ... use DviStart() to start on core 1
// - system clock must be set to 252 MHz
void DviInit()
{
	// clear frame buffer
#if USE_FRAMEBUF		// use default display frame buffer
	memset(FrameBuf, 0, FRAMESIZE*sizeof(FRAMETYPE));
#endif

	// clear back buffer
#if USE_FRAMEBUF && (BACKBUFSIZE > 0)
	memset(BackBuf, 0, BACKBUFSIZE*sizeof(FRAMETYPE));
#endif

	// clear data buffer with gray color
	int i;
	u32* d = DviLineBuf;
	for (i = count_of(DviLineBuf); i > 0; i--) *d++ = 0x5fd80;

	// initialize parameters
	DviScanLine = 0; // currently processed scanline
	DviBufInx = 0; // at first, control buffer 1 will be sent out
	DviFrame = 0; // current frame

	// next control buffer
	DviCtrlBufNext[0] = DviLineBufSync;
	DviCtrlBufNext[1] = DviLineBufDark12;
	DviCtrlBufNext[2] = &DviLineBufDark12[2*4];

	// initialize PIO
	DviPioInit();

	// initialize PWM
	DviPwmInit();

	// initialize control buffers
	DviBufInit();

	// DVI initialize DMA
	DviDmaInit();

	// enable DMA IRQ
	NVIC_IRQEnable(IRQ_DMA_1);

	// start DMA
	DMA_Start(DVI_DMA_CB(0));
	DMA_Start(DVI_DMA_CB(1));
	DMA_Start(DVI_DMA_CB(2));

#if DVI_IRQTIME				// debug flag - measure delta time of DVI service
	DviTimeTmp = Time();
#endif // DVI_IRQTIME

	// DVI output enable
	DviEnable();
}

// terminate DVI ... use DviStop() to stop on core 1
void DviTerm()
{
	// DVI output disable
	DviDisable();

	// abort DMA channels (interrupt data channel first and then control channel)
	DMA_Abort(DVI_DMA_DB0); // pre-abort, could be chaining right now
	DMA_Abort(DVI_DMA_CB0);
	DMA_Abort(DVI_DMA_DB0);
	DMA_Abort(DVI_DMA_CB1);
	DMA_Abort(DVI_DMA_DB1);
	DMA_Abort(DVI_DMA_CB2);
	DMA_Abort(DVI_DMA_DB2);

	// disable IRQ1 from DMA0
	NVIC_IRQDisable(IRQ_DMA_1);
	DMA_IRQ1Disable(DVI_DMA_DB0);

	// Clear the interrupt request for DMA control channel
	DMA_IRQ1Clear(DVI_DMA_DB0);

	// reset PIO
	PioInit(DVI_PIO);
}

#define DVI_REQNO 	0	// request - no
#define DVI_REQINIT     1	// request - init
#define DVI_REQTERM	2	// request - terminate
volatile int DviReq = DVI_REQNO;	// current DVI request

// DVI core
void NOFLASH(DviCore)()
{
	void (*fnc)();
	int req;

	// infinite loop
	while (True)
	{
		// data memory barrier
		dmb();

		// initialize/terminate VGA
		req = DviReq;
		if (req != DVI_REQNO)
		{
			if (req == DVI_REQINIT)
			{
//				VgaInit(); // initialize
				DviInit(); // initialize
			}
			else
			{
				DviTerm(); // terminate
//				VgaTerm();
			}

			DviReq = DVI_REQNO;
		}		
	}
}

// start DVI on core 1 from core 0 (must be paired with DviStop())
// - system clock must be set to 252 MHz
void DviStart()
{
	// run DVI core
	Core1Exec(DviCore);

	// initialize DVI
	DviReq = DVI_REQINIT;
	dmb();
	while (DviReq != DVI_REQNO) { dmb(); }
}

// terminate DVI on core 1 from core 0 (must be paired with DviStart())
void DviStop()
{
	// terminate DVI
	DviReq = DVI_REQTERM;
	dmb();
	while (DviReq != DVI_REQNO) { dmb(); }

	// core 1 reset
	Core1Reset();
}

#endif // USE_DVI
