PLUGIN_INSTALLDIR=/usr/local/lib/gkrellm/plugins

LOCALEDIR ?= /usr/share/locale

export PLUGIN_INSTALLDIR LOCALEDIR

enable_nls=1
export enable_nls

all:
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} )

alsa5:
	(cd po && ${MAKE} all)
	(cd src && ${MAKE} alsa5=1)

install:
	(cd po && ${MAKE} install)
	(cd src && ${MAKE} install)

clean:
	(cd po && ${MAKE} clean)
	(cd src && ${MAKE} clean)

help:
	@echo ""
	@echo "make [without-alsa=yes] [without-esd=yes]"
	@echo ""
	@echo "    ALSA and Esound support (if found) are both compiled in"
	@echo "    by default.  Use a without-xxx option to override."
	@echo "    Either fftw3 or fftw2 will be used, with preference to fftw3."
	@echo ""
