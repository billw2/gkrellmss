INSTALLROOT ?= $(DESTDIR)$(PREFIX)

ifeq ($(INSTALLROOT),)
	INSTALLROOT = /usr/local
endif

INSTALLDIR ?= $(INSTALLROOT)/lib/gkrellm2/plugins
INSTALL ?= install


GTK_INCLUDE = `pkg-config gtk+-2.0 --cflags`
GTK_LIB = `pkg-config gtk+-2.0 --libs`

OS_NAME=$(shell uname -s)

SD_INCLUDE ?= 
SD_LIB ?= 


ifeq ($(without-alsa),yes)
	CONFIGURE_ARGS += --without-alsa
endif
ifeq ($(without-esd),yes)
	CONFIGURE_ARGS += --without-esd
endif

DUMMY_VAR := $(shell ./configure $(CONFIGURE_ARGS))
HAVE_FFTW3 = $(shell grep -c HAVE_FFTW3 configure.h)
HAVE_FFTW2 = $(shell grep -c HAVE_FFTW2 configure.h)
HAVE_ALSA = $(shell grep -c HAVE_ALSA configure.h)
HAVE_ESOUND = $(shell grep -c HAVE_ESOUND configure.h)

ifneq ($(HAVE_ALSA),1)
ifneq ($(HAVE_ESOUND),1)
$(error Can't configure ALSA or Esound, see src/configure*)
endif
endif

ifneq ($(HAVE_ALSA),1)
ifneq ($(without-alsa),yes)
WARN = $(warning Note: could not configure ALSA support.)
endif
endif

ifneq ($(HAVE_ESOUND),1)
ifneq ($(without-esd),yes)
WARN = $(warning Note: could not configure Esound support.)
endif
endif

ifeq ($(HAVE_FFTW3),1)
	FFTW_LIB ?= -lfftw3
endif

ifeq ($(HAVE_FFTW2),1)
	FFTW_LIB ?= -lrfftw -lfftw
endif

ifeq ($(HAVE_ALSA),1)
	SD_LIB += -lasound
endif

ifeq ($(HAVE_ESOUND),1)
	SD_INCLUDE += `esd-config --cflags`
	SD_LIB += `esd-config --libs`
endif


FLAGS = -O2 -Wall -fPIC $(GTK_INCLUDE) $(SD_INCLUDE)
LIBS = $(GTK_LIB) $(SD_LIB) $(FFTW_LIB) -lm

ifeq ($(OS_NAME),Darwin)
LFLAGS = -bundle -bundle_loader `which gkrellm`
STRIP =
FLAGS += -DDARWIN
else
LFLAGS = -shared
STRIP ?= -s
endif

ifeq ($(debug),1)
	FLAGS += -g
endif

ifeq ($(enable_nls),1)
	FLAGS += -DENABLE_NLS -DLOCALEDIR=\"$(LOCALEDIR)\"
endif

ifeq ($(alsa),1)
	FLAGS += -DGKRELLM_ALSA
endif

CC = gcc $(FLAGS) $(CFLAGS)

OBJS = gkrellmss.o oscope.o spectrum.o sdlib.o option.o

all:	gkrellmss.so

warn: ; $(WARN)

gkrellmss.so: $(OBJS) warn
	$(CC) $(OBJS) -o gkrellmss.so $(LFLAGS) $(LIBS)


clean:
	rm -f *.o configure.h configure.log core *.so* *.bak *~

install: 
	$(INSTALL) -d -m 755 $(INSTALLDIR)
	$(INSTALL) -c $(STRIP) -m 755 gkrellmss.so $(INSTALLDIR)

help:
	@echo ""
	@echo "make [without-alsa=yes] [without-esd=yes]"
	@echo ""
	@echo "    ALSA and Esound support (if found) are both compiled in"
	@echo "    by default.  Use a without-xxx option to override."
	@echo "    Either fftw3 or fftw2 will be used, with preference to fftw3."
	@echo ""

gkrellmss.o: gkrellmss.c gkrellmss.h configure
oscope.o: oscope.c gkrellmss.h configure
spectrum.o: spectrum.c gkrellmss.h configure
sdlib.o: sdlib.c gkrellmss.h sdlib-esd.c sdlib-alsa.c configure
option.o: option.c gkrellmss.h configure
