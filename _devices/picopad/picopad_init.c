
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
#include "../../_lib/inc/lib_pwmsnd.h"
#include "../../_display/minivga/minivga.h"
#include "../../_display/st7789/st7789.h"
#include "../../_lib/inc/lib_fat.h"
#include "../../_lib/inc/lib_sd.h"
#include "picopad_bat.h"
#include "picopad_key.h"
#include "picopad_led.h"

// DEBUG indicator - copy it into program checkpoint
// { GPIO_Init(LED1_PIN); GPIO_DirOut(LED1_PIN); while (1) { GPIO_Flip(LED1_PIN); io32 i; for (i = 1000000; i > 0; i--); } }

// Device init
void DeviceInit()
{
#if USE_MINIVGA					// use mini-VGA display with simple frame buffer
	// start VGA on CPU 1 (must be paired with VgaStop)
	VgaStart();
#else
	// initialize LEDs
	LedInit();

	// initialize display
	DispInit(DISP_ROT);
#endif

	// initilize keys
	KeyInit();

	// init battery measurement
	BatInit();

#if USE_PWMSND		// use PWM sound output; set 1.. = number of channels (lib_pwmsnd.c, lib_pwmsnd.h)
	// initialize PWM sound output (must be re-initialized after changing CLK_SYS system clock)
	PWMSndInit();
#endif

#if USE_SD		// use SD card (lib_sd.c, lib_sd.h)
	// initialize SD card interface (must be re-initialized after changing system clock)
	SDInit();
#endif
}

// Device terminate
void DeviceTerm()
{
#if USE_MINIVGA					// use mini-VGA display with simple frame buffer
	// terminate VGA on CPU 1 (must be paired with VgaStart)
	VgaStop();
#else
	// terminate LEDs
	LedTerm();

	// terminate display
	DispTerm();
#endif
	// terminate keys
	KeyTerm();

	// terminate battery measurement
	BatTerm();

#if USE_PWMSND	// use PWM sound output; set 1.. = number of channels (lib_pwmsnd.c, lib_pwmsnd.h)
	// terminate PWM sound output
	PWMSndTerm();
#endif

#if USE_SD		// use SD card (lib_sd.c, lib_sd.h)
	// terminate SD card interface
	SDTerm();
#endif
}
