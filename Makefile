##
## This file is part of the coreboot project.
##
## Copyright (C) 2008 Advanced Micro Devices, Inc.
## Copyright (C) 2008 Uwe Hermann <uwe@hermann-uwe.de>
## Copyright (C) 2009-2010 coresystems GmbH
## Copyright (C) 2011 secunet Security Networks AG
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
## 1. Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
## 2. Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
## 3. The name of the author may not be used to endorse or promote products
##    derived from this software without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
## ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
## ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
## FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
## DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
## OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
## HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
## LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
## OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
## SUCH DAMAGE.
##

ifeq ($(INNER_SCANBUILD),y)
CC_real:=$(CC)
endif

$(if $(wildcard .xcompile),,$(eval $(shell bash util/xcompile/xcompile $(XGCCPATH) > .xcompile)))
include .xcompile

ifeq ($(INNER_SCANBUILD),y)
CC:=$(CC_real)
HOSTCC:=$(CC_real) --hostcc
HOSTCXX:=$(CC_real) --hostcxx
endif

export top := $(CURDIR)
export src := src
export srck := $(top)/util/kconfig
export obj ?= build
export objutil ?= $(obj)/util
export objk := $(objutil)/kconfig


export KCONFIG_AUTOHEADER := $(obj)/config.h
export KCONFIG_AUTOCONFIG := $(obj)/auto.conf

# directory containing the toplevel Makefile.inc
TOPLEVEL := .

CONFIG_SHELL := sh
KBUILD_DEFCONFIG := configs/defconfig
UNAME_RELEASE := $(shell uname -r)
DOTCONFIG ?= .config
KCONFIG_CONFIG = $(DOTCONFIG)
export KCONFIG_CONFIG
HAVE_DOTCONFIG := $(wildcard $(DOTCONFIG))
MAKEFLAGS += -rR --no-print-directory

# Make is silent per default, but 'make V=1' will show all compiler calls.
Q:=@
ifneq ($(V),1)
ifneq ($(Q),)
.SILENT:
endif
endif

# Disable implicit/built-in rules to make Makefile errors fail fast.
.SUFFIXES:

HOSTCC = gcc
HOSTCXX = g++
HOSTCFLAGS := -g
HOSTCXXFLAGS := -g

# Pass -undef to avoid predefined legacy macros like 'i386'
PREPROCESS_ONLY := -E -P -x assembler-with-cpp -undef

DOXYGEN := doxygen
DOXYGEN_OUTPUT_DIR := doxygen

all: real-all bootable

# This include must come _before_ the pattern rules below!
# Order _does_ matter for pattern rules.
include util/kconfig/Makefile

# Three cases where we don't need fully populated $(obj) lists:
# 1. when no .config exists
# 2. when make config (in any flavour) is run
# 3. when make distclean is run
# Don't waste time on reading all Makefile.incs in these cases
ifeq ($(strip $(HAVE_DOTCONFIG)),)
NOCOMPILE:=1
endif
ifneq ($(MAKECMDGOALS),)
ifneq ($(filter %config %clean,$(MAKECMDGOALS)),)
NOCOMPILE:=1
endif
ifeq ($(MAKECMDGOALS), %clean)
NOMKDIR:=1
endif
endif

ifeq ($(NOCOMPILE),1)
include $(TOPLEVEL)/Makefile.inc
real-all: config

else

include $(HAVE_DOTCONFIG)

include toolchain.inc

COMMA:=,

# Function to wrap calls to the linker (will be overridden for CLANG)
# $1 stage
# $2 objects to link (will be wrapped in --start-group and --end-group)
# $3 options passed directly (to GCC or LD) (-nostdlib and -static are default)
# $4 options passed to LD (wrapped with -Wl, for GCC)
link=$(CC_$(1)) $(CFLAGS_$(1)) -nostdlib -static $(3) $(foreach opt,$(4),-Wl$(COMMA)$(opt)) -Wl,--start-group $(2) -Wl,--end-group

ifneq ($(INNER_SCANBUILD),y)
ifeq ($(CONFIG_COMPILER_LLVM_CLANG),y)
CC:=clang -m32 -mno-mmx -mno-sse
HOSTCC:=clang
link:=$(LD_$(1)) -nostdlib -static $(3) $(4) --start-group $(2) --end-group
endif
endif

ifeq ($(CONFIG_CCACHE),y)
CCACHE:=$(word 1,$(wildcard $(addsuffix /ccache,$(subst :, ,$(PATH)))))
ifeq ($(CCACHE),)
$(error ccache selected, but not found in PATH)
endif
CCACHE:=CCACHE_COMPILERCHECK=content CCACHE_BASEDIR=$(top) $(CCACHE)
CC := $(CCACHE) $(CC)
HOSTCC := $(CCACHE) $(HOSTCC)
HOSTCXX := $(CCACHE) $(HOSTCXX)
ROMCC := $(CCACHE) $(ROMCC)
endif

strip_quotes = $(subst ",,$(subst \",,$(1)))

# The primary target needs to be here before we include the
# other files

ifeq ($(INNER_SCANBUILD),y)
CONFIG_SCANBUILD_ENABLE:=
endif

ifeq ($(CONFIG_SCANBUILD_ENABLE),y)
ifneq ($(CONFIG_SCANBUILD_REPORT_LOCATION),)
CONFIG_SCANBUILD_REPORT_LOCATION:=-o $(CONFIG_SCANBUILD_REPORT_LOCATION)
endif
real-all:
	echo '#!/bin/sh' > .ccwrap
	echo 'CC="$(CC)"' >> .ccwrap
	echo 'if [ "$$1" = "--hostcc" ]; then shift; CC="$(HOSTCC)"; fi' >> .ccwrap
	echo 'if [ "$$1" = "--hostcxx" ]; then shift; CC="$(HOSTCXX)"; fi' >> .ccwrap
	echo 'eval $$CC $$*' >> .ccwrap
	chmod +x .ccwrap
	scan-build $(CONFIG_SCANBUILD_REPORT_LOCATION) -analyze-headers --use-cc=$(top)/.ccwrap --use-c++=$(top)/.ccwrap $(MAKE) INNER_SCANBUILD=y
else
real-all: real-target
endif

# must come rather early
.SECONDEXPANSION:

$(obj)/config.h:
	$(MAKE) oldconfig

# Dependencies that should be built before all files in all classes
# Careful: interpreted as order-only prerequisites, use only for header files!
generic-deps = $(obj)/config.h

# Every file can append to this string. It is simply eval'ed after the scan.
postprocessors :=

# Add a new class of source/object files to the build system
add-class= \
	$(eval $(1)-srcs:=) \
	$(eval $(1)-objs:=) \
	$(eval classes+=$(1))

# Special classes are managed types with special behaviour
# On parse time, for each entry in variable $(1)-y
# a handler $(1)-handler is executed with the arguments:
# * $(1): directory the parser is in
# * $(2): current entry
add-special-class= \
	$(eval $(1):=) \
	$(eval special-classes+=$(1))

# Clean -y variables, include Makefile.inc
# Add paths to files in X-y to X-srcs
# Add subdirs-y to subdirs
includemakefiles= \
	$(foreach class,classes subdirs $(classes) $(special-classes), $(eval $(class)-y:=)) \
	$(eval -include $(1)) \
	$(foreach class,$(classes-y), $(call add-class,$(class))) \
	$(foreach class,$(classes), \
		$(eval $(class)-srcs+= \
			$$(subst $(top)/,, \
			$$(abspath $$(subst $(dir $(1))/,/,$$(addprefix $(dir $(1)),$$($(class)-y))))))) \
	$(foreach special,$(special-classes), \
		$(foreach item,$($(special)-y), $(call $(special)-handler,$(dir $(1)),$(item)))) \
	$(eval subdirs+=$$(subst $(CURDIR)/,,$$(abspath $$(addprefix $(dir $(1)),$$(subdirs-y)))))

# For each path in $(subdirs) call includemakefiles
# Repeat until subdirs is empty
evaluate_subdirs= \
	$(eval cursubdirs:=$(subdirs)) \
	$(eval subdirs:=) \
	$(foreach dir,$(cursubdirs), \
		$(eval $(call includemakefiles,$(dir)/Makefile.inc))) \
	$(if $(subdirs),$(eval $(call evaluate_subdirs)))

# collect all object files eligible for building
subdirs:=$(TOPLEVEL)
$(eval $(call evaluate_subdirs))
ifeq ($(FAILBUILD),1)
$(error cannot continue build)
endif

# Eliminate duplicate mentions of source files in a class
$(foreach class,$(classes),$(eval $(class)-srcs:=$(sort $($(class)-srcs))))

# Converts one or more source file paths to their corresponding build/ paths.
# Only .c and .S get converted to .o, other files (like .ld) keep their name.
# $1 stage name
# $2 file path (list)
src-to-obj=$(foreach file,$(2),$(subst .$(1),,$(basename $(patsubst src/%,$(obj)/%,$(file)))).$(1)$(patsubst %.c,%.o,$(patsubst %.S,%.o,$(suffix $(file)))))

$(foreach class,$(classes),$(eval $(class)-objs+=$(call src-to-obj,$(class),$($(class)-srcs))))

# Call post-processors if they're defined
$(eval $(postprocessors))

allsrcs:=$(foreach var, $(addsuffix -srcs,$(classes)), $($(var)))
allobjs:=$(foreach var, $(addsuffix -objs,$(classes)), $($(var)))
alldirs:=$(sort $(abspath $(dir $(allobjs))))

# macro to define template macros that are used by use_template macro
define create_cc_template
# $1 obj class
# $2 source suffix (c, S, ld)
# $3 additional compiler flags
# $4 additional dependencies
ifn$(EMPTY)def $(1)-objs_$(2)_template
de$(EMPTY)fine $(1)-objs_$(2)_template
$$(call src-to-obj,$1,$$(1)): $$(1) $(4) | $$$$(generic-deps)
	@printf "    CC         $$$$(subst $$$$(obj)/,,$$$$(@))\n"
	$(CC_$(1)) -MMD $$$$(CFLAGS_$(1)) -MT $$$$(@) $(3) -c -o $$$$@ $$$$<
en$(EMPTY)def
end$(EMPTY)if
endef

filetypes-of-class=$(subst .,,$(sort $(suffix $($(1)-srcs))))
$(foreach class,$(classes), \
	$(foreach type,$(call filetypes-of-class,$(class)), \
		$(eval $(class)-$(type)-ccopts += $(generic-$(type)-ccopts) $($(class)-generic-ccopts)) \
		$(eval $(call create_cc_template,$(class),$(type),$($(class)-$(type)-ccopts),$($(class)-$(type)-deps)))))

foreach-src=$(foreach file,$($(1)-srcs),$(eval $(call $(1)-objs_$(subst .,,$(suffix $(file)))_template,$(file))))
$(eval $(foreach class,$(classes),$(call foreach-src,$(class))))

DEPENDENCIES += $(addsuffix .d,$(basename $(allobjs)))
-include $(DEPENDENCIES)

printall:
	@$(foreach class,$(classes),echo $(class)-objs=$($(class)-objs); )
	@echo alldirs:=$(alldirs)
	@echo allsrcs=$(allsrcs)
	@echo DEPENDENCIES=$(DEPENDENCIES)
	@echo LIBGCC_FILE_NAME=$(LIBGCC_FILE_NAME_$(class))
	@$(foreach class,$(special-classes),echo $(class):='$($(class))'; )

endif

ifndef NOMKDIR
$(shell mkdir -p $(obj) $(objutil)/kconfig/lxdialog $(additional-dirs) $(alldirs))
endif

cscope:
	cscope -bR

doxy: doxygen
doxygen:
	$(DOXYGEN) documentation/Doxyfile.coreboot

doxyclean: doxygen-clean
doxygen-clean:
	rm -rf $(DOXYGEN_OUTPUT_DIR)

clean-for-update: doxygen-clean clean-for-update-target
	rm -rf $(obj) .xcompile

clean: clean-for-update clean-target
	rm -f .ccwrap

clean-cscope:
	rm -f cscope.out

distclean: clean
	rm -f .config .config.old ..config.tmp .kconfig.d .tmpconfig* .ccwrap .xcompile

.PHONY: $(PHONY) clean clean-for-update clean-cscope cscope distclean doxygen doxy .xcompile
