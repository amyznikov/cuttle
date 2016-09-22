platform=android-16
DESTDIR =
prefix  = /usr
incdir  = $(prefix)/include
libdir  = $(prefix)/lib
pcdir   = $(libdir)/pkgconfig
pc      = $(pcdir)/$(LIBNAME).pc


ndk_root=$(shell dirname $(shell command -v ndk-build))
ifeq ($(ndk_root),)
$(error Can not clocate NDK root. Is NDK installed correctly?) 
endif

SYSROOT=$(ndk_root)/platforms/${platform}/arch-arm
DESTDIR=$(SYSROOT)

CC = $(shell ndk-which gcc) -std=gnu99
CFLAGS = -Wall -Wextra -Wno-missing-field-initializers -O3 -g --sysroot="$(SYSROOT)" -D__ANDROID__=1

