#!/bin/make
#
# Entropy key daemon package
#
# Copyright 2011 Simtec Electronics
#
# For licence terms refer to the COPYING file.

# Override any of these on the cmdline to set options.
# Ensure they are overridden during both build and install.
DESTDIR ?=
PREFIX ?= /usr
DOCPREFIX ?= ${PREFIX}/share/doc/ekeyd
MUNINPLUGINS ?= ${PREFIX}/share/munin/plugins
MUNINPLUGINSCONF ?= /etc/munin/plugin-conf.d

#optional daemons
BUILD_ULUSBD ?= no
BUILD_EGDLINUX ?= no

# extra install docs
DOCFILES := README.FreeBSD README README.protocol README.Centos5 README.security
ifneq ($(BUILD_EGDLINUX),no)
DOCFILES += README.egd-protocol README.egd-linux
endif

.PHONY: all host install clean

all: host

host:
	${MAKE} -C host BUILD_ULUSB=${BUILD_ULUSBD} BUILD_EGDLINUX=${BUILD_EGDLINUX} DESTDIR=${DESTDIR}

clean:
	${MAKE} -C host BUILD_ULUSB=${BUILD_ULUSBD} BUILD_EGDLINUX=${BUILD_EGDLINUX} DESTDIR=${DESTDIR}  clean

install:
	${MAKE} -C host BUILD_ULUSB=$(BUILD_ULUSBD) BUILD_EGDLINUX=$(BUILD_EGDLINUX) DESTDIR=${DESTDIR} install
	for DOC in $(DOCFILES) ; do \
	  install -D -m 644 doc/$$DOC $(DESTDIR)/$(DOCPREFIX)/$$DOC ; \
	done
	install -D -m 755 munin/ekeyd_stat_ $(DESTDIR)/$(MUNINPLUGINS)/ekeyd_stat_
	install -D -m 644 munin/plugin-conf.d_ekeyd $(DESTDIR)/${MUNINPLUGINSCONF}/ekeyd

installBSD:
	${MAKE} -C host BUILD_ULUSB=$(BUILD_ULUSBD) BUILD_EGDLINUX=$(BUILD_EGDLINUX) DESTDIR=${DESTDIR} install
	install -d $(DESTDIR)/$(DOCPREFIX)/
	for DOC in $(DOCFILES); do \
	  install -m 644 doc/$$DOC $(DESTDIR)/$(DOCPREFIX)/$$DOC ; \
	done
	install -d $(DESTDIR)/$(MUNINPLUGINS)/
	install -m 755 munin/ekeyd_stat_ $(DESTDIR)/$(MUNINPLUGINS)/ekeyd_stat_
	install -d $(DESTDIR)/${MUNINPLUGINSCONF}/
	install -m 644 munin/plugin-conf.d_ekeyd $(DESTDIR)/${MUNINPLUGINSCONF}/ekeyd
