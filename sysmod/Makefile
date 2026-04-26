#---------------------------------------------------------------------------------
# Makefile para Switch Engine SysModule
# Gera exefs.nsp instalavel em sdmc:/atmosphere/contents/<TID>/
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Defina DEVKITPRO no seu ambiente.")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
APP_TITLE        := switch-engine-mod
APP_AUTHOR       := you
APP_VERSION      := 0.1.0

# DEVE bater com o "title_id" no .json
TARGET_TID       := 420000000053454E

TARGET           := switch-engine-mod
BUILD            := build
SOURCES          := source
INCLUDES         := source ../include
EXEFS_SRC        := exefs_src

NPDM_JSON        := $(TOPDIR)/switch-engine-mod.json

#---------------------------------------------------------------------------------
ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS  := -g -Wall -Wextra -O2 -ffunction-sections \
           $(ARCH) $(DEFINES) $(INCLUDE) -D__SWITCH__

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++20

ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS    := -lnx
LIBDIRS := $(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
    export LD := $(CC)
else
    export LD := $(CXX)
endif

export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_SRC)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all install

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nso $(TARGET).elf exefs.nsp

#---------------------------------------------------------------------------------
# install: copia para o seu SD card montado em $(SDMOUNT) (ajuste no .env)
#---------------------------------------------------------------------------------
SDMOUNT ?= /run/media/$(USER)/SDCARD

install: all
	@mkdir -p $(SDMOUNT)/atmosphere/contents/$(TARGET_TID)/flags
	@cp exefs.nsp $(SDMOUNT)/atmosphere/contents/$(TARGET_TID)/exefs.nsp
	@cp toolbox.json $(SDMOUNT)/atmosphere/contents/$(TARGET_TID)/toolbox.json
	@touch $(SDMOUNT)/atmosphere/contents/$(TARGET_TID)/flags/boot2.flag
	@echo "Instalado em $(SDMOUNT)/atmosphere/contents/$(TARGET_TID)/"

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# Regras de build do sysmod:
#   .elf -> elf2nso -> main (NSO)
#   .json -> npdmtool -> main.npdm
#   build_pfs0 main + main.npdm -> exefs.nsp
#---------------------------------------------------------------------------------
all: ../exefs.nsp

../exefs.nsp: exefs/main exefs/main.npdm
	@build_pfs0 exefs ../exefs.nsp
	@echo built ... exefs.nsp

exefs/main: $(OUTPUT).elf
	@mkdir -p exefs
	@elf2nso $(OUTPUT).elf exefs/main

exefs/main.npdm: $(NPDM_JSON)
	@mkdir -p exefs
	@npdmtool $(NPDM_JSON) exefs/main.npdm
	@echo built ... main.npdm

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
