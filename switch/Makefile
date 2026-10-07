# Nintendo Switch build: make -C switch (devkitPro). Based on the
# switch-examples application template.
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

TARGET      := OutRun2006-NX
BUILD       := build
SOURCES     := source ../src/driving ../src/platform ../src/runtime ../src/system ../src/enhancements ../src/ports
SHADERS     := shaders
DATA        :=
INCLUDES    := ../src
ROMFS       := romfs
OUT_SHADERS := shaders
APP_TITLE   := OutRun 2006 NX Fidelity
APP_AUTHOR  := OutRunSwitch
APP_VERSION := r155

ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
# Optimisation (port performance): -O3 with link-time optimisation; the FP
# semantics stay strict (-ffp-contract=off -fno-fast-math below).
OPTIMISE ?= -O3 -flto=8
NOBULK ?= 0
CFLAGS := -g -Wall $(OPTIMISE) -ffunction-sections -fdata-sections $(ARCH) $(DEFINES)
CFLAGS += $(INCLUDE) -D__SWITCH__ -DOR2_HAVE_VORBIS -DOUTRUN_WITH_FFMPEG
ifeq ($(NOBULK),1)
CFLAGS += -DOR2_NO_BULK
endif
# Reconstructed code still uses checked exceptions; do not inherit the template's -fno-exceptions.
CXXFLAGS := $(CFLAGS) -std=gnu++17 -fno-rtti -ffp-contract=off -fno-fast-math
ASFLAGS := -g $(ARCH)
LDFLAGS = -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) $(OPTIMISE) -Wl,-Map,$(notdir $*.map)
LIBS := -ldeko3d -lvorbisfile -lvorbis -logg -lavformat -lavcodec -lswscale -lswresample -lavutil -ldav1d -lbz2 -lnx -lz -lm
LIBDIRS := $(PORTLIBS) $(LIBNX)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES) $(SHADERS),$(CURDIR)/$(dir)) $(CURDIR)/../src $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp))) asset_views.cpp pc_pmt.cpp
# NOBULK=1: without the bulk translation (bulk_tr.cpp, 2100 functions never called in play).
# The console faults in libnx consoleInit when the image (text + data + bss) reaches ~57 MB
# (2026-10-06; 51.9 MB boots): the cold translated modules are compiled -Os (pragma at the
# top of each *_tr.cpp) to stay under it.
ifeq ($(NOBULK),1)
CPPFILES := $(filter-out bulk_tr.cpp,$(CPPFILES))
endif
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
GLSLFILES:= $(notdir $(wildcard $(SHADERS)/*.glsl))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifneq ($(strip $(ROMFS)),)
ROMFS_TARGETS :=
ROMFS_FOLDERS :=
ifneq ($(strip $(OUT_SHADERS)),)
ROMFS_SHADERS := $(ROMFS)/$(OUT_SHADERS)
ROMFS_TARGETS += $(patsubst %.glsl,$(ROMFS_SHADERS)/%.dksh,$(GLSLFILES))
ROMFS_FOLDERS += $(ROMFS_SHADERS)
endif
export ROMFS_DEPS := $(foreach file,$(ROMFS_TARGETS),$(CURDIR)/$(file))
endif

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ICON)),)
icons := $(wildcard *.jpg)
ifneq (,$(findstring $(TARGET).jpg,$(icons)))
export APP_ICON := $(TOPDIR)/$(TARGET).jpg
else
ifneq (,$(findstring icon.jpg,$(icons)))
export APP_ICON := $(TOPDIR)/icon.jpg
endif
endif
else
export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_ICON)),)
export NROFLAGS += --icon=$(APP_ICON)
endif
ifeq ($(strip $(NO_NACP)),)
export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp
endif
ifneq ($(ROMFS),)
export NROFLAGS += --romfsdir=$(CURDIR)/$(ROMFS)
endif

.PHONY: $(BUILD) clean all
all: $(ROMFS_TARGETS) | $(BUILD)

$(BUILD): $(ROMFS_TARGETS)
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

ifneq ($(strip $(ROMFS_TARGETS)),)
$(ROMFS_TARGETS): | $(ROMFS_FOLDERS)

$(ROMFS_FOLDERS):
	@mkdir -p $@

$(ROMFS_SHADERS)/%_vsh.dksh: %_vsh.glsl
	@echo {vert} $(notdir $<)
	@uam -s vert -o $@ $<

$(ROMFS_SHADERS)/%_fsh.dksh: %_fsh.glsl
	@echo {frag} $(notdir $<)
	@uam -s frag -o $@ $<
endif

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(ROMFS_FOLDERS) $(TARGET).nro $(TARGET).nacp $(TARGET).elf

else
.PHONY: all
DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).nro

ifeq ($(strip $(NO_NACP)),)
$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp $(ROMFS_DEPS)
else
$(OUTPUT).nro: $(OUTPUT).elf $(ROMFS_DEPS)
endif

$(OUTPUT).elf: $(OFILES)
$(OFILES_SRC): $(HFILES_BIN)

%.bin.o %_bin.h: %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)
endif
