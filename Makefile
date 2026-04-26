#---------------------------------------------------------------------------------
# Makefile para Switch Engine - Memory Scanner (Tesla Overlay)
# Baseado no template oficial libnx + libtesla
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Defina a variavel DEVKITPRO no seu ambiente. export DEVKITPRO=<path to>devkitPro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# Configuracao do projeto
#---------------------------------------------------------------------------------
APP_TITLE        := Switch Engine
APP_AUTHOR       := you
APP_VERSION      := 0.1.0

TARGET           := switch-engine
BUILD            := build
SOURCES          := source source/gui source/scanner source/util
DATA             := data
INCLUDES         := source include
EXEFS_SRC        := exefs_src

#---------------------------------------------------------------------------------
# Flags
#
# Notas importantes para um overlay Tesla valido:
#  - -fPIE: NRO precisa ser position-independent.
#  - -specs=$(DEVKITPRO)/libnx/switch.specs: traz o linker script + crt0
#    que produzem um ELF que elf2nro converte em NRO valido.
#  - -Wl,--build-id=sha1: nx-ovlloader+ usa o build-id em alguns paths,
#    inofensivo se ignorado.
#  - NAO usamos -shared. Overlays Tesla NAO sao .so dinamicos: sao NROs
#    (Nintendo Relocatable Object) com main() -- exatamente o mesmo formato
#    de um homebrew .nro normal, apenas com extensao ".ovl" e localizado em
#    sdmc:/switch/.overlays/. O nx-ovlloader+ carrega NROs, NAO NSOs --
#    NSOs sao usados por sysmodules (carregados pelo Atmosphere/pm).
#---------------------------------------------------------------------------------
ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS  := -g -Wall -Wextra -O2 -ffunction-sections \
           $(ARCH) $(DEFINES)

CFLAGS  += $(INCLUDE) -D__SWITCH__

CXXFLAGS := $(CFLAGS) -fno-exceptions -std=gnu++20

ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) \
           -Wl,-Map,$(notdir $*.map) \
           -Wl,--build-id=sha1

LIBS    := -lnx

#---------------------------------------------------------------------------------
# NROFLAGS: passa o .nacp para o elf2nro embutir o metadata (titulo/autor/versao)
# dentro do NRO. Sem isso o overlay carrega mas pode aparecer "sem nome" ou
# ser filtrado pelo Ultrahand na hora de listar.
# (A regra %.nro: %.elf vem do switch_rules e roda 'elf2nro $< $@ $(NROFLAGS)'.)
#
# Importante: usamos $(TOPDIR) (raiz do projeto) e NAO $(CURDIR) -- esta
# variavel e' reavaliada quando o sub-make roda dentro de build/, onde CURDIR
# vira build/ e o caminho do nacp fica errado. TOPDIR e' exportado e fica
# fixo na raiz em ambos os contextos.
#---------------------------------------------------------------------------------
ifneq ($(strip $(APP_TITLE)),)
    NROFLAGS += --nacp=$(TOPDIR)/$(TARGET).nacp
endif

#---------------------------------------------------------------------------------
# Bibliotecas (libtesla como submodulo em lib/libtesla)
#---------------------------------------------------------------------------------
LIBDIRS := $(PORTLIBS) $(LIBNX) $(TOPDIR)/lib/libtesla

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)

export NROFLAGS
export APP_TITLE
export APP_AUTHOR
export APP_VERSION

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
    export LD := $(CC)
else
    export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export OUTPUT   := $(CURDIR)/$(TARGET)

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).ovl $(TARGET).nro $(TARGET).nacp $(TARGET).nso $(TARGET).elf

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# Regras de build
# Tesla overlays sao NROs renomeados para .ovl. NAO sao NSOs!
#   ELF -> elf2nro -> .nro -> cp -> .ovl
# A regra "%.nro: %.elf %.nacp" vem do switch_rules da libnx, e a regra
# "%.nacp: " usa APP_TITLE/APP_AUTHOR/APP_VERSION definidos la em cima.
#---------------------------------------------------------------------------------
all: $(OUTPUT).ovl

$(OUTPUT).ovl: $(OUTPUT).nro
	@cp $< $(OUTPUT).ovl
	@echo "Built $(notdir $@)"

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp

$(OUTPUT).elf: $(OFILES)

$(OFILES_SRC): $(HFILES_BIN)

%.bin.o %_bin.h: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
