
// ****************************************************************************
//
//                            DemoVGA Screen Shot
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

#ifndef _DEMOVGA_SS_H
#define _DEMOVGA_SS_H

#ifdef __cplusplus
extern "C" {
#endif

#if USE_SCREENSHOT		// use screen shots

// Do one screen shot (generates file SCRxxxxx.BMP in root of SD card)
void ScreenShot();

// Do one small screen shot (generates file SCRxxxxx.BMP in root of SD card)
void SmallScreenShot();

#endif // USE_SCREENSHOT		// use screen shots

#ifdef __cplusplus
}
#endif

#endif // _DEMOVGA_SS_H
