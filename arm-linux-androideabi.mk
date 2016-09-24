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



SYSROOT=$(NDK_ROOT)/platforms/${NDK_PLATFORM}/arch-arm
DESTDIR=$(SYSROOT)

CC = $(shell ndk-which gcc) -std=gnu99
CFLAGS = -march=armv7-a -Wall -Wextra -Wno-missing-field-initializers -O3 -g --sysroot="$(SYSROOT)" -D__ANDROID__=1 

MODULE_DIR = src/ucontext/arm
include $(MODULE_DIR)/ucontext.mk
