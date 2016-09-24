NDK_PLATFORM=android-16

NDK_ROOT=$(shell dirname $(shell command -v ndk-build))
ifeq ($(NDK_ROOT),)
$(error Can not clocate NDK root. Is NDK installed correctly?) 
endif

prefix  = /usr
incdir  = $(prefix)/include
libdir  = $(prefix)/lib
pcdir   = $(libdir)/pkgconfig
pc      = $(pcdir)/$(LIBNAME).pc

SYSROOT ?= $(NDK_ROOT)/platforms/${NDK_PLATFORM}/arch-arm
DESTDIR ?= $(SYSROOT)

CC = $(shell ndk-which gcc) -std=gnu99 --sysroot="$(SYSROOT)"
CFLAGS = -march=armv7-a -Wall -Wextra -Wno-missing-field-initializers -O3 -g  -D__ANDROID__=1 

SRCDIRS += $(wildcard src/android/*)
CFLAGS += $(addprefix -I,$(wildcard src/android/*))


SUBMODULE_DIR = src/ucontext/arm
include $(SUBMODULE_DIR)/ucontext.mk
