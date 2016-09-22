DESTDIR =
prefix  = /usr/local
incdir  = $(prefix)/include
libdir  = $(prefix)/lib
pcdir   = $(libdir)/pkgconfig
pc      = $(pcdir)/$(LIBNAME).pc

CC = gcc -std=gnu99
CFLAGS = -Wall -Wextra -Wno-missing-field-initializers -O3 -g

