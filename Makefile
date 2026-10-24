#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
# GRAPHICS is a list of directories containing graphics files
# GFXBUILD is the directory where converted graphics files will be placed
#   If set to $(BUILD), it will statically link in the converted
#   files as if they were data files.
#
# NO_SMDH: if set to anything, no SMDH file is generated.
# ROMFS is the directory which contains the RomFS, relative to the Makefile (Optional)
# APP_TITLE is the name of the app stored in the SMDH file (Optional)
# APP_DESCRIPTION is the description of the app stored in the SMDH file (Optional)
# APP_AUTHOR is the author of the app stored in the SMDH file (Optional)
# ICON is the filename of the icon (.png), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.png
#     - icon.png
#     - <libctru folder>/default_icon.png
#---------------------------------------------------------------------------------
# `make`           dev build -> slider3ds.3dsx: SELECT-at-launch updater (pulls from DEV_HOST) and
#                  debug controls/overlay (SLIDER_DEV=1).
# `make RELEASE=1` release build in its own build dir -> slider3ds_release.3dsx, without either.
ifeq ($(strip $(RELEASE)),)
TARGET		:=	slider3ds
BUILD		:=	build
else
TARGET		:=	slider3ds_release
BUILD		:=	build-release
endif
APP_TITLE	:=	Slider
APP_DESCRIPTION	:=	Slide chunks of the world to solve puzzles
APP_AUTHOR	:=	boomo
# Both builds' icon (made by tools/make_icon.py; unset, the release would look for slider3ds_release.png).
ICON		:=	slider3ds.png
# Every folder directly under src/game holds sources (a new one is picked up automatically; no
# deeper nesting and no files directly in src/game: CMake uses the same rule).
SOURCES		:=	src $(patsubst %/,%,$(sort $(dir $(wildcard src/game/*/*.cpp)))) src/platform/ctr
DATA		:=	data
INCLUDES	:=	src
GRAPHICS	:=	gfx
GFXBUILD	:=	$(BUILD)
ROMFS		:=	romfs
#GFXBUILD	:=	$(ROMFS)/gfx

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -Wextra -O2 -mword-relocations \
			-ffunction-sections \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -D__3DS__

# Dev PC serving the build for the SELECT-at-launch updater (src/platform/ctr/dev_update.cpp).
# Changing DEV_HOST needs a `make clean` (compiler flags aren't dependency-tracked).
ifeq ($(strip $(RELEASE)),)
DEV_HOST	?=	10.0.0.200
CFLAGS	+=	-DSLIDER_DEV=1 -DDEV_UPDATE_URL='"http://$(DEV_HOST):8000/slider3ds.3dsx"'
else
CFLAGS	+=	-DSLIDER_DEV=0
endif

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lvorbisidec -logg -lcitro2d -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) $(CTRULIB)


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))

# Objects are named by basename only, so two sources with the same name would collide.
ALLSRCNAMES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp $(dir)/*.c $(dir)/*.s)))
ifneq ($(words $(ALLSRCNAMES)),$(words $(sort $(ALLSRCNAMES))))
$(error Source files in $(SOURCES) share a basename: $(ALLSRCNAMES))
endif
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))
GFXFILES	:=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
ifeq ($(GFXBUILD),$(BUILD))
#---------------------------------------------------------------------------------
export T3XFILES :=  $(GFXFILES:.t3s=.t3x)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
export ROMFS_T3XFILES	:=	$(patsubst %.t3s, $(GFXBUILD)/%.t3x, $(GFXFILES))
export T3XHFILES		:=	$(patsubst %.t3s, $(BUILD)/%.h, $(GFXFILES))
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_SOURCES 	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES)) \
			$(PICAFILES:.v.pica=.shbin.o) $(SHLISTFILES:.shlist=.shbin.o) \
			$(addsuffix .o,$(T3XFILES))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES	:=	$(PICAFILES:.v.pica=_shbin.h) $(SHLISTFILES:.shlist=_shbin.h) \
			$(addsuffix .h,$(subst .,_,$(BINFILES))) \
			$(GFXFILES:.t3s=.h)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS	:=	$(if $(NO_SMDH),,$(OUTPUT).smdh)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

#---------------------------------------------------------------------------------
# Exported game data (assets/, from the Unity exporter) -> RomFS:
#   assets/sheets/<name>.png -> romfs/gfx/<name>.t3x   (tex3ds, in the lossless format named by
#                               assets/sheets/<name>.fmt: la8 for gray sheets, else rgba8; incl. the
#                               font atlas, tools/make_font.py)
#   assets/<dir>/*.json      -> romfs/<dir>/*.json    (copied: areas, fonts, audio/audio.json)
#   assets/audio/*.ogg, *.adp, *.pcm -> romfs/audio/  (copied: tools/make_audio.py's renders)
# The .3dsx embeds RomFS, so it is rebuilt when any of these change.
#---------------------------------------------------------------------------------
SHEET_PNGS	:=	$(wildcard assets/sheets/*.png)
ROMFS_SHEETS	:=	$(patsubst assets/sheets/%.png,$(ROMFS)/gfx/%.t3x,$(SHEET_PNGS))
AREA_JSONS	:=	$(wildcard assets/*/*.json)
ROMFS_JSONS	:=	$(patsubst assets/%,$(ROMFS)/%,$(AREA_JSONS))
AUDIO_FILES	:=	$(wildcard assets/audio/*.ogg assets/audio/*.adp assets/audio/*.pcm)
ROMFS_AUDIO	:=	$(patsubst assets/%,$(ROMFS)/%,$(AUDIO_FILES))
export _3DSXDEPS	+=	$(addprefix $(CURDIR)/,$(ROMFS_SHEETS) $(ROMFS_JSONS) $(ROMFS_AUDIO))

.PHONY: all clean cia

#---------------------------------------------------------------------------------
all: $(BUILD) $(GFXBUILD) $(DEPSDIR) $(ROMFS_T3XFILES) $(T3XHFILES) $(ROMFS_SHEETS) $(ROMFS_JSONS) $(ROMFS_AUDIO)
	@stale=$$(for f in $(ROMFS)/audio/*; do [ -e "$$f" ] || continue; [ -e "assets/audio/$${f##*/}" ] || echo "$$f"; done); \
	if [ -n "$$stale" ]; then echo "removing stale" $$stale; rm -f $$stale $(TARGET).3dsx; fi # (renders make_audio no longer writes)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

ifneq ($(GFXBUILD),$(BUILD))
$(GFXBUILD):
	@mkdir -p $@
endif

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

#---------------------------------------------------------------------------------
# `make RELEASE=1 cia` -> slider3ds.cia: the release build, installable from the HOME Menu (FBI), its
# banner tools/cia/banner.png + banner.wav (tools/make_banner.py's placeholder: replace banner.png with
# 256x128 art) and makerom spec tools/cia/cia.rsf. Needs makerom and bannertool (devkitPro's tools/bin).
# `make cia` -> slider3ds_dev.cia: the dev build as a CIA (its own title id: both install side by side),
# to read the overlay of an installed title (e.g. the audio thread's core). Its SELECT-at-launch updater
# still writes the .3dsx, not the CIA.
ifeq ($(strip $(RELEASE)),)
CIA_NAME	:=	slider3ds_dev.cia
CIA_UNIQUE_ID	:=	0xF51D4
CIA_PRODUCT	:=	CTR-P-SLDD
else
CIA_NAME	:=	slider3ds.cia
CIA_UNIQUE_ID	:=	0xF51D3
CIA_PRODUCT	:=	CTR-P-SLDR
endif
cia: all
	@bannertool makebanner -i tools/cia/banner.png -a tools/cia/banner.wav -o $(BUILD)/banner.bnr
	@makerom -f cia -o $(CIA_NAME) -target t -exefslogo -elf $(TARGET).elf -rsf tools/cia/cia.rsf \
		-icon $(TARGET).smdh -banner $(BUILD)/banner.bnr -DAPP_TITLE="$(APP_TITLE)" -DAPP_PRODUCT_CODE=$(CIA_PRODUCT) \
		-DAPP_UNIQUE_ID=$(CIA_UNIQUE_ID) -DAPP_ROMFS=$(ROMFS) -DAPP_CATEGORY=Application -DAPP_USE_ON_SD=true \
		-DAPP_ENCRYPTED=false -DAPP_MEMORY_TYPE=Application -DAPP_SYSTEM_MODE=64MB -DAPP_SYSTEM_MODE_EXT=Legacy \
		-DAPP_CPU_SPEED=268MHz -DAPP_ENABLE_L2_CACHE=false -DAPP_VERSION_MAJOR=0
	@echo built ... $(CIA_NAME)

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(CIA_NAME) $(OUTPUT).smdh $(TARGET).elf $(GFXBUILD) $(ROMFS)/gfx $(ROMFS)/*/*.json $(ROMFS)/audio

#---------------------------------------------------------------------------------
$(ROMFS)/gfx/%.t3x	:	assets/sheets/%.png assets/sheets/%.fmt
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	@echo $(notdir $<) $$(cat assets/sheets/$*.fmt)
	@tex3ds -f $$(cat assets/sheets/$*.fmt) -z auto -o $@ $<

#---------------------------------------------------------------------------------
$(ROMFS)/%.json	:	assets/%.json
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	@cp $< $@

#---------------------------------------------------------------------------------
$(ROMFS)/audio/%	:	assets/audio/%
#---------------------------------------------------------------------------------
	@mkdir -p $(dir $@)
	@cp $< $@

#---------------------------------------------------------------------------------
$(GFXBUILD)/%.t3x	$(BUILD)/%.h	:	%.t3s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@tex3ds -i $< -H $(BUILD)/$*.h -d $(DEPSDIR)/$*.d -o $(GFXBUILD)/$*.t3x

#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).3dsx	:	$(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf	:	$(OFILES)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
.PRECIOUS	:	%.t3x %.shbin
#---------------------------------------------------------------------------------
%.t3x.o	%_t3x.h :	%.t3x
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

#---------------------------------------------------------------------------------
%.shbin.o %_shbin.h : %.shbin
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
