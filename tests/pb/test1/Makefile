############################################################
#
# cuttlefish Makefile
# Generated by amyznikov Aug 31, 2016
#   from 'linux-gcc-executable' template
#
############################################################

SHELL = /bin/bash

TARGET = nanopb-test

all: $(TARGET)


cross   =
sysroot =
DESTDIR =
prefix  = /usr/local
bindir  = $(prefix)/bin
incdir  = $(prefix)/include
libdir  = $(prefix)/lib

INCLUDES+= -I. -I../../../include
SOURCES = $(wildcard *.c)
HEADERS = $(wildcard *.h)
MODULES = $(foreach s,$(SOURCES),$(addsuffix .o,$(basename $(s))))


# C preprocessor flags
CPPFLAGS=$(DEFINES) $(INCLUDES)

# C Compiler and flags
CC = $(cross)gcc -std=gnu99
CFLAGS= -Wall -Wextra -Wno-missing-field-initializers -O3 -g3

# Loader Flags And Libraries
LD=$(CC)
LDFLAGS = $(CFLAGS)

# STRIP = $(cross)strip --strip-all
STRIP = @echo "don't strip "

LIBCUTTLE = ../../../libcuttle.a 

LDLIBS += $(LIBCUTTLE) -L/usr/local/lib -lcrypto -lssl -lrt -ldl -lpthread


#########################################


$(MODULES): $(HEADERS) Makefile
$(TARGET) : $(MODULES) Makefile $(LIBCUTTLE)
	$(LD) $(LDFLAGS)  $(MODULES) $(LDLIBS) -o $@

clean:
	$(RM) $(MODULES)

distclean: clean
	$(RM) $(TARGET)

install: $(TARGET) $(DESTDIR)/$(bindir)
	cp $(TARGET) $(DESTDIR)/$(bindir) && $(STRIP) $(DESTDIR)/$(bindir)/$(TARGET)

uninstall:
	$(RM) $(DESTDIR)/$(bindir)/$(TARGET)


$(DESTDIR)/$(bindir):
	mkdir -p $@
