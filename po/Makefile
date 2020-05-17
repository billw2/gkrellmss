# Makefile for locale directory

CWD = po
MSGFMT = msgfmt
SUB_DIRS = 
FILES_PO:=$(wildcard *.po)
FILES_MO:=$(FILES_PO:.po=.mo)

LOCALEDIR ?= /usr/share/locale

ifeq ($(enable_nls),1)
all: mo-files

mo-files: $(FILES_MO)

install: 
	$(MAKE) all
	for f in $(FILES_MO) ; do mkdir -p \
		$(INSTALL_PREFIX)$(LOCALEDIR)/`basename $$f .mo`/LC_MESSAGES ; \
		cp $$f $(INSTALL_PREFIX)$(LOCALEDIR)/`basename $$f .mo`/LC_MESSAGES/gkrellm-gkrellmss.mo ; done

%.mo: %.po
	$(MSGFMT) -f -v -o $@ $<

else

all:

install:

endif

clean:
	$(RM) $(FILES_MO)

