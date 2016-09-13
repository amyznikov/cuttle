############################################################
#
# libcuttle Makefile
# 	Generated by amyznikov at Aug 26, 2016
#
############################################################

SHELL=/bin/bash

LIBNAME=cuttle
VERSION = 0.0.1

LIB=lib$(LIBNAME).a

all: lib

cross   =
sysroot =
DESTDIR =
prefix  = /usr/local
incdir  = $(prefix)/include
libdir  = $(prefix)/lib
pcdir   = $(libdir)/pkgconfig
pc      = $(pcdir)/$(LIBNAME).pc

installdirs = $(addprefix $(DESTDIR)/,$(prefix) $(incdir) $(libdir) $(pcdir))
$(installdirs):
	mkdir -p $@


# preprocessor flags
CPPFLAGS := -Iinclude -Isrc -I/usr/include/postgresql -I/usr/local/include/postgresql $(CPPFLAGS)

# C Compiler and flags
CC ?= $(cross)gcc -std=c99
CFLAGS=-Wall -Wextra -O3 -g3

# C++ Compiler and flags
CXX ?= $(cross)gcc -std=c++11
CXXFLAGS=$(CFLAGS)


#########################################

HEADERS += $(wildcard include/cuttle/*.h include/cuttle/*/*.h)
INTERNAL_HEADERS += $(wildcard src/*.h src/*/*.h)

SOURCES += $(wildcard src/*.c src/*/*.c)


MODULES  =  $(addsuffix .o,$(basename $(SOURCES)))
$(MODULES): $(HEADERS) $(INTERNAL_HEADERS)

lib : $(LIB)($(MODULES))
clean : ; $(RM) $(MODULES)
distclean: clean ; $(RM) $(LIB)


install: $(LIB) $(installdirs)
	cp $(LIB) $(DESTDIR)/$(libdir)/
	cp -r include/* $(DESTDIR)/$(incdir)/
	echo -e "" \
		"prefix=$(prefix)\n" \
		"exec_prefix=$(prefix)\n" \
		"libdir=$(libdir)\n" \
		"includedir=$(incdir)\n" \
		"\n" \
		"Name: $(LIBNAME)\n" \
		"Description: $(LIBNAME) library\n" \
		"Version: $(VERSION)\n" \
		"Requires:\n" \
		"Requires.private:\n" \
		"Conflicts:\n" \
		"Libs: -L$(libdir) -l$(LIBNAME)\n" \
		"Libs.private:\n" \
		"Cflags: -I$(incdir)\n" \
		> $(DESTDIR)/$(pc)


uninstall:
	$(RM) $(DESTDIR)/$(libdir)/$(LIB)
	$(RM) $(DESTDIR)/$(pc)
	$(RM) $(addprefix $(DESTDIR)/$(prefix)/,$(HEADERS))
	
	

test:
	@echo "CC=$(CC)"
	@echo "CXX=$(CXX)"
	@echo "CFLAGS=$(CFLAGS)"
	@echo "CXXFLAGS=$(CXXFLAGS)"
	@echo "CPPFLAGS=$(CPPFLAGS)"
	@echo "LDFLAGS=$(LDFLAGS)"
	@echo "SOURCES=$(SOURCES)"
	@echo "HEADERS=$(HEADERS)"
	@echo "MODULES=$(MODULES)"