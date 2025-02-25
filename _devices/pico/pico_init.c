
// ****************************************************************************
//
//                              PicoPad Init
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
#include "../../_display/minivga/minivga.h" // VGA display
#include "../../_display/dvi/dvi.h" // DVI display
#include "../../_sdk/inc/sdk_gpio.h"
#include "pico_init.h"
#include "pico_bat.h"

// Device init
void DeviceInit()
{
#if USE_DVI
	// start DVI on CPU 1 (must be paired with DviStop)
//	DviStart();
#endif

#if USE_EXTDISP
	// start VGA on CPU 1 (must be paired with VgaStop)
//	VgaStart();
#else
	// init battery measurement
	BatInit();
#endif
}

// Device terminate
void DeviceTerm()
{
#if USE_DVI
	// terminate DVI on CPU 1 (must be paired with DviStart)
//	DviStop();
#endif

#if USE_EXTDISP
	// terminate VGA on CPU 1 (must be paired with VgaStart)
//	VgaStop();
#else
	// terminate battery measurement
	BatTerm();
#endif
}

// set LED ON (inx = LED index LED?)
void LedOn(u8 inx)
{
	if (inx == LED1) GPIO_Out1(LED_PIN);
}

// set LED OFF (inx = LED index LED?)
void LedOff(u8 inx)
{
	if (inx == LED1) GPIO_Out0(LED_PIN);
}

// flip LED (inx = LED index LED?)
void LedFlip(u8 inx)
{
	if (inx == LED1) GPIO_Flip(LED_PIN);
}

// set LED (inx = LED index LED?)
void LedSet(u8 inx, u8 val)
{
	if (val == 0) LedOff(inx); else LedOn(inx);
}
