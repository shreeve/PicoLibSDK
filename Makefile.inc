
# Includes from main Makefile.
# Set parameters TARGET, DEVICE and DEVCLASS, as follows.

# Command to setup target project name (i.e. name of output file).
# Name "LOADER" is reserved for boot loader.
#    set TARGET=HELLOW

# Command to select target device (use names from the _setup.bat file).
#    set DEVICE=picopad10

# Command to select class of target device (to use common compilation setup).
#    set DEVCLASS=picopad

# Command to select linker script NO_FLASH ('noflash' = use only RAM, #define flag NO_FLASH)
#    set MEMMAP=noflash


##############################################################################
# Input files

# ASM source files
ifeq (${MEMMAP},noflash)
# NOFLASH - no loader if located in RAM
DEFINE += -D NO_FLASH=1 -D PICO_NO_FLASH=1
else
ifeq (${TARGET},LOADER)
ASRC += ../../../_boot2/boot2_w25q080_bin.S
else
ifeq (${DEVCLASS},pico)
ASRC += ../../../_boot2/boot2_w25q080_bin.S
else
ASRC += ../../../_loader/loader_${DEVICE}.S
endif
endif
endif

# SDK
include ../../../_sdk/_makefile.inc

# Libraries
include ../../../_lib/_makefile.inc

# fonts
include ../../../_font/_makefile.inc

# Device class
include ../../../_devices/${DEVCLASS}/_makefile.inc

# Includes
IPATH += -I. -I../../../_sdk/

##############################################################################
# Configuration

# temporary build directory
TEMP = ./build

# Base makefile (to ensure recompilation after change)
MAKEFILE = ./Makefile

# Base include file (to ensure recompilation after change)
INCLUDEFILE = ./include.h

# Base config file (to ensure recompilation after change)
CONFIGFILE = ./config.h

# linker script
ifeq (${TARGET},LOADER)
LDSCRIPT = ../../../memmap_boot.ld
else
ifeq (${MEMMAP},noflash)
LDSCRIPT = ../../../memmap_noflash.ld
else
ifeq (${DEVCLASS},pico)
LDSCRIPT = ../../../memmap_default.ld
else
LDSCRIPT = ../../../memmap_app.ld
endif
endif
endif

# CPU Architecture
MARCH = armv6-m

# CPU
MCU = cortex-m0plus

##############################################################################
# Compilation flags

# define MCU
ARCHCFG = -march=${MARCH} -mcpu=${MCU} -mthumb

# === ASM flags

# MCU
AFLAGS = $(ARCHCFG)

# === C flags

# MCU
CFLAGS = $(ARCHCFG)

# Optimization level
#CFLAGS += -O0	# do no optimization
#CFLAGS += -O1	# optimize minimally
#CFLAGS += -O2	# optimize more
#CFLAGS += -O3	# optimize even more
#CFLAGS += -Ofast # optimize for speed
#CFLAGS += -Og -g3 # optimize for debugger (use together with -g0..-g3, level of debugging information)
CFLAGS += -Os	# optimize for size

# Do not use built-in functions. This is case when compiller changes printf("x") to putchar('x').
#CFLAGS += -fno-builtin

# create separate sections (to enable eliminate unused sections)
CFLAGS += -ffunction-sections -fdata-sections 

# use unsigned char (this is default for ARM cpu)
CFLAGS += -funsigned-char

# pedantic - to report error when 'return' is missing
#CFLAGS += -Wall -pedantic

# generate debug informations
#CFLAGS += -g3

# warning as errors
CFLAGS += -Werror

# === C++ flags

CPPFLAGS = $(CFLAGS)

# do not use exceptions and unwind frames
CPPFLAGS += -fno-exceptions -fno-unwind-tables
CPPFLAGS += -fno-rtti -fno-use-cxa-atexit -std=gnu++17

# === Linker flags

# MCU
LDFLAGS = $(ARCHCFG)

# no built in
LDFLAGS += -Wl,--build-id=none

# libc without system functions
LDFLAGS += --specs=nosys.specs 

# generate map file
LDFLAGS += -Wl,-Map=$(TARGET).map

# Linker script
LDFLAGS += -Wl,-script=$(LDSCRIPT)

# eliminate unused sections
LDFLAGS += -Wl,--gc-sections

# no start files
LDFLAGS += -nostartfiles

# wrap library functions
LDFLAGS+=-Wl,--wrap=__popcountsi2 -Wl,--wrap=__popcountdi2
LDFLAGS+=-Wl,--wrap=__clz -Wl,--wrap=__clzl -Wl,--wrap=__clzll
LDFLAGS+=-Wl,--wrap=__aeabi_idiv -Wl,--wrap=__aeabi_idivmod -Wl,--wrap=__aeabi_ldivmod
LDFLAGS+=-Wl,--wrap=__aeabi_uidiv -Wl,--wrap=__aeabi_uidivmod -Wl,--wrap=__aeabi_uldivmod
LDFLAGS+=-Wl,--wrap=__aeabi_dadd -Wl,--wrap=__aeabi_ddiv -Wl,--wrap=__aeabi_dmul -Wl,--wrap=__aeabi_drsub
LDFLAGS+=-Wl,--wrap=__aeabi_dsub -Wl,--wrap=__aeabi_cdcmpeq -Wl,--wrap=__aeabi_cdrcmple
LDFLAGS+=-Wl,--wrap=__aeabi_cdcmple -Wl,--wrap=__aeabi_dcmpeq -Wl,--wrap=__aeabi_dcmplt
LDFLAGS+=-Wl,--wrap=__aeabi_dcmple -Wl,--wrap=__aeabi_dcmpge -Wl,--wrap=__aeabi_dcmpgt
LDFLAGS+=-Wl,--wrap=__aeabi_dcmpun -Wl,--wrap=__aeabi_i2d -Wl,--wrap=__aeabi_l2d
LDFLAGS+=-Wl,--wrap=__aeabi_ui2d -Wl,--wrap=__aeabi_ul2d -Wl,--wrap=__aeabi_d2iz
LDFLAGS+=-Wl,--wrap=__aeabi_d2lz -Wl,--wrap=__aeabi_d2uiz -Wl,--wrap=__aeabi_d2ulz
LDFLAGS+=-Wl,--wrap=__aeabi_d2f -Wl,--wrap=sqrt -Wl,--wrap=cos -Wl,--wrap=sin -Wl,--wrap=tan
LDFLAGS+=-Wl,--wrap=atan2 -Wl,--wrap=exp -Wl,--wrap=log -Wl,--wrap=ldexp -Wl,--wrap=copysign
LDFLAGS+=-Wl,--wrap=trunc -Wl,--wrap=floor -Wl,--wrap=ceil -Wl,--wrap=round -Wl,--wrap=sincos
LDFLAGS+=-Wl,--wrap=asin -Wl,--wrap=acos -Wl,--wrap=atan -Wl,--wrap=sinh -Wl,--wrap=cosh
LDFLAGS+=-Wl,--wrap=tanh -Wl,--wrap=asinh -Wl,--wrap=acosh -Wl,--wrap=atanh -Wl,--wrap=exp2
LDFLAGS+=-Wl,--wrap=log2 -Wl,--wrap=exp10 -Wl,--wrap=log10 -Wl,--wrap=pow -Wl,--wrap=powint
LDFLAGS+=-Wl,--wrap=hypot -Wl,--wrap=cbrt -Wl,--wrap=fmod -Wl,--wrap=drem -Wl,--wrap=remainder
LDFLAGS+=-Wl,--wrap=remquo -Wl,--wrap=expm1 -Wl,--wrap=log1p -Wl,--wrap=fma -Wl,--wrap=__aeabi_lmul
LDFLAGS+=-Wl,--wrap=__aeabi_fadd -Wl,--wrap=__aeabi_fdiv -Wl,--wrap=__aeabi_fmul
LDFLAGS+=-Wl,--wrap=__aeabi_frsub -Wl,--wrap=__aeabi_fsub -Wl,--wrap=__aeabi_cfcmpeq
LDFLAGS+=-Wl,--wrap=__aeabi_cfrcmple -Wl,--wrap=__aeabi_cfcmple -Wl,--wrap=__aeabi_fcmpeq
LDFLAGS+=-Wl,--wrap=__aeabi_fcmplt -Wl,--wrap=__aeabi_fcmple -Wl,--wrap=__aeabi_fcmpge
LDFLAGS+=-Wl,--wrap=__aeabi_fcmpgt -Wl,--wrap=__aeabi_fcmpun -Wl,--wrap=__aeabi_i2f
LDFLAGS+=-Wl,--wrap=__aeabi_l2f -Wl,--wrap=__aeabi_ui2f -Wl,--wrap=__aeabi_ul2f
LDFLAGS+=-Wl,--wrap=__aeabi_f2iz -Wl,--wrap=__aeabi_f2lz -Wl,--wrap=__aeabi_f2uiz
LDFLAGS+=-Wl,--wrap=__aeabi_f2ulz -Wl,--wrap=__aeabi_f2d -Wl,--wrap=sqrtf -Wl,--wrap=cosf
LDFLAGS+=-Wl,--wrap=sinf -Wl,--wrap=tanf -Wl,--wrap=atan2f -Wl,--wrap=expf -Wl,--wrap=logf
LDFLAGS+=-Wl,--wrap=ldexpf -Wl,--wrap=copysignf -Wl,--wrap=truncf -Wl,--wrap=floorf
LDFLAGS+=-Wl,--wrap=ceilf -Wl,--wrap=roundf -Wl,--wrap=sincosf -Wl,--wrap=asinf
LDFLAGS+=-Wl,--wrap=acosf -Wl,--wrap=atanf -Wl,--wrap=sinhf -Wl,--wrap=coshf -Wl,--wrap=tanhf
LDFLAGS+=-Wl,--wrap=asinhf -Wl,--wrap=acoshf -Wl,--wrap=atanhf -Wl,--wrap=exp2f -Wl,--wrap=log2f
LDFLAGS+=-Wl,--wrap=exp10f -Wl,--wrap=log10f -Wl,--wrap=powf -Wl,--wrap=powintf -Wl,--wrap=hypotf
LDFLAGS+=-Wl,--wrap=cbrtf -Wl,--wrap=fmodf -Wl,--wrap=dremf -Wl,--wrap=remainderf -Wl,--wrap=remquof
LDFLAGS+=-Wl,--wrap=expm1f -Wl,--wrap=log1pf -Wl,--wrap=fmaf -Wl,--wrap=malloc -Wl,--wrap=calloc
LDFLAGS+=-Wl,--wrap=free -Wl,--wrap=realloc
LDFLAGS+=-Wl,--wrap=printf -Wl,--wrap=vprintf
LDFLAGS+=-Wl,--wrap=sprintf -Wl,--wrap=snprintf -Wl,--wrap=vsnprintf -Wl,--wrap=vsprintf
LDFLAGS+=-Wl,--wrap=__clzsi2 -Wl,--wrap=__clzdi2 -Wl,--wrap=__ctzsi2 -Wl,--wrap=__ctzdi2
LDFLAGS+=-Wl,--wrap=puts -Wl,--wrap=putchar -Wl,--wrap=getchar
LDFLAGS+=-Wl,--wrap=memset -Wl,--wrap=__aeabi_memset -Wl,--wrap=__aeabi_memset4 -Wl,--wrap=__aeabi_memset8
LDFLAGS+=-Wl,--wrap=memcpy -Wl,--wrap=__aeabi_memcpy -Wl,--wrap=__aeabi_memcpy4 -Wl,--wrap=__aeabi_memcpy8

##############################################################################
# Compiler

# compiler base
COMP = arm-none-eabi-

# C compiler name
CC = ${COMP}gcc

# C++ compiler name
CPP = ${COMP}g++

# ASM compiler name
AS = ${COMP}gcc

# Linker
LD = ${COMP}g++

# Object dump
OD = ${COMP}objdump

# Object copy
OC = ${COMP}objcopy

# Names
NM = ${COMP}nm

# Size
SZ = ${COMP}size

# uf2
ifeq (${OS},Windows_NT)
UF = ../../../_tools/elf2uf2.exe
else
UF = ../../../_tools/elf2uf2
endif

##############################################################################
# File list

# list of all source files
ALLSRC = $(ASRC) $(CSRC) $(SRC)

# list of all files without directory and without extension
ALLBASE = $(notdir $(basename $(ALLSRC)))

# list of all object files *.o
ALLOBJ = $(addprefix $(TEMP)/, $(addsuffix .o, $(ALLBASE)))

##############################################################################
# Clear temporary directory (use "make clean")

clean:
	@../../../_tools/rm -rf $(TEMP)
	@../../../_tools/rm -f ./$(TARGET).bin ./$(TARGET).hex ./$(TARGET).lst ./$(TARGET).map ./$(TARGET).sym ./$(TARGET).siz ./$(TARGET).uf2

##############################################################################
# Compile all (use "make all")

all: createdirs elf bin hex lst sym siz uf2

##############################################################################
# Create temporary directory

createdirs:
ifeq (${OS},Windows_NT)
	@../../../_tools/mkdir.exe -p $(TEMP)
else
	@mkdir -p $(TEMP)
endif

##############################################################################
# Dependencies of output files

elf: $(TEMP)/$(TARGET).elf
bin: ./$(TARGET).bin
hex: ./$(TARGET).hex
lst: ./$(TARGET).lst
sym: ./$(TARGET).sym
siz: ./$(TARGET).siz
uf2: ./$(TARGET).uf2

##############################################################################
# Create .bin file from .elf file

./$(TARGET).bin: $(TEMP)/$(TARGET).elf
	@echo     bin		 $@
	@$(OC) -O binary $< $@

##############################################################################
# Create .hex file from .elf file

./$(TARGET).hex: $(TEMP)/$(TARGET).elf
	@echo     hex		 $@
	@$(OC) -O ihex $< $@

##############################################################################
# Create .lst file from .elf file

./$(TARGET).lst: $(TEMP)/$(TARGET).elf
	@echo     lst		 $@
	@$(OD) -d -S $< > $@

##############################################################################
# Create .sym file from .elf file

./$(TARGET).sym: $(TEMP)/$(TARGET).elf
	@echo     sym		 $@
	@$(NM) -n $< > $@

##############################################################################
# Create .siz file from .elf file

./$(TARGET).siz: $(TEMP)/$(TARGET).elf
	@echo     siz		 $@
	@$(SZ) $< > $@

##############################################################################
# Create .uf2 file from .elf file

./$(TARGET).uf2: $(TEMP)/$(TARGET).elf
	@echo     uf2		 $@
	@$(UF) $< $@

##############################################################################
# Create .o files from .c files

define CC_TEMP
$(TEMP)/$(notdir $(basename $(1))).o : $(1) $(MAKEFILE) $(INCLUDEFILE) $(CONFIGFILE)
	@echo     CC		 $$<
	@$(CC) $$(DEFINE) $$(IPATH) $$(CFLAGS) -std=gnu11 -c $$< -o $$@
endef

$(foreach src, $(CSRC), $(eval $(call CC_TEMP, $(src))))

##############################################################################
# Create .o files from .cpp files

define CPP_TEMP
$(TEMP)/$(notdir $(basename $(1))).o : $(1) $(MAKEFILE) $(INCLUDEFILE) $(CONFIGFILE)
	@echo     C++		 $$<
	@$(CPP) $$(DEFINE) $$(IPATH) $$(CPPFLAGS) -c $$< -o $$@
endef

$(foreach src, $(SRC), $(eval $(call CPP_TEMP, $(src))))

##############################################################################
# Create .o files from .S files

define AS_TEMP
$(TEMP)/$(notdir $(basename $(1))).o : $(1) $(MAKEFILE) $(INCLUDEFILE) $(CONFIGFILE)
	@echo     ASM		 $$<
	@$(CC) $$(DEFINE) $$(IPATH) $$(AFLAGS) -c $$< -o $$@
endef

$(foreach src, $(ASRC), $(eval $(call AS_TEMP, $(src))))

##############################################################################
# Create .elf from .o files

$(TEMP)/$(TARGET).elf: $(ALLOBJ) $(MAKEFILE) $(LDSCRIPT)
	@echo     ld		 $@
	@$(LD) $(LDFLAGS) $(ALLOBJ) -o $@

##############################################################################
# Head dependencies, to recompile C after changing H

ifneq (${MAKECMDGOALS},clean)
-include $(wildcard $(TEMP)/*.d)
endif

##############################################################################
# List of dependencies not creating files

.PHONY: clean all createdirs elf bin hex lst sym siz uf2
