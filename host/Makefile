#!/bin/make
#
# Entropy key daemon and tools
#
# Copyright 2009-2011 Simtec Electronics
#
# For licence terms refer to the COPYING file.


# Override any of these on the cmdline to set options. 
# Ensure they are overridden during both build and install.
DESTDIR ?=
PREFIX ?= /usr
SYSCONFPREFIX ?= /etc/entropykey
RUNTIMEPREFIX ?= /var/run
MANPREFIX ?= $(PREFIX)/share/man/man
BUILD_ULUSBD ?= no
BUILD_EGDLINUX ?= no
RM ?= rm -f
LUA_V ?= 5.1
EXTRA_INC ?=
LUA_INC ?= -I/usr/include/lua5.1
LIBDL ?= -ldl
PTHFLAGS ?=
PTHLIBS ?= -lpthread
KERNOUTOK :=
KERNOUTNOTOK := -- 
EGDSOCK := /etc/entropy
UDEVOK :=
MANZCMD := gzip -c
MANZEXT := .gz

# Current tool version as returned from tools
EKEYD_VERSION_S="1.1.4"

# Attempt to detect an OS and set build options as appropriate
OSNAME=$(shell uname -s | tr A-Z a-z | tr -d /)

ifeq ($(OSNAME),linux)
CFLAGS += -DEKEY_OS_LINUX -DEKEY_FULL_TERMIOS
override BUILD_EGDLINUX:=yes
else
ifeq ($(OSNAME),gnukfreebsd)
# Bizarre hybrid GNU/kFreeBSD platform is sort-of-GNU sort-of-FreeBSD
# So far, only seen in Debian's distribution.
CFLAGS += -DEKEY_OS_FREEBSD
# Incomplete termios support means no -DEKEY_FULL_TERMIOS
# FreeBSD's kernel lacks a kernel mode output
override KERNOUTOK := -- 
override KERNOUTNOTOK :=
# Lua is, however, likely installed in the usual places, so no need to fudge those.
else
ifeq ($(OSNAME),mirbsd)
CFLAGS += -DEKEY_OS_$(shell echo $(OSNAME) | tr a-z A-Z)
# Build the userland USBd support for older versions (recent MirBSD has support)
override BUILD_ULUSBD:=yes
override PTHFLAGS:=-pthread
override PTHLIBS:=
# System dependent settings
override LUA_INC:=
override LUA_V:=
override LIBDL:=
override OPT:=
override UDEVOK:=-- 
override MANZCMD:=cat
override MANZEXT:=
# see RAND_egd(3)
override EGDSOCK:=/var/run/egd-pool
ifeq ($(IN_MIRPORTS),)
# standalone compilation
INCLUDES += -I/usr/mpkg/include
LIBDIRS += -L/usr/mpkg/lib
endif
else
# Other BSDs ?
ifeq ($(findstring bsd,$(OSNAME)),bsd)
# *BSD doesn't have obvious cdc-acm support (may need recompilation of kernel) so offer
# the somewhat more hacky userland USBd support via libusb for now.
CFLAGS += -DEKEY_OS_$(shell echo $(OSNAME) | tr a-z A-Z)
override BUILD_ULUSBD:=yes
ifeq ($(OSNAME),freebsd)
override LUA_V:=-5.1
# FreeBSD's kernel lacks a kernel mode output
override KERNOUTOK := -- 
override KERNOUTNOTOK :=
endif
ifeq ($(OSNAME),openbsd)
override LUA_V:=
override PTHFLAGS:=-pthread
override PTHLIBS:=
override MANZCMD:=cat
override MANZEXT:=
endif
override LUA_INC:=-I/usr/local/include/lua51
override LIBDL:=
LIBDIRS += -L/usr/local/lib
INCLUDES += -I/usr/local/include
endif
endif
endif
endif

# Set up tool flags
INCLUDES += -I../device/frames/ -I../device/ -I../device/skein/ $(LUA_INC) $(EXTRA_INC)
OPT ?= -O2

CFLAGS += $(INCLUDES)
CFLAGS += -g -Wall $(OPT)
CFLAGS += -fno-strict-aliasing
CFLAGS += -std=c99 -Wall -pedantic -Wshadow -Werror -D_GNU_SOURCE
CFLAGS += '-DCONFIGFILE="$(SYSCONFPREFIX)/ekeyd.conf"'
CFLAGS += '-DPIDFILE="$(RUNTIMEPREFIX)/ekeyd.pid"'
CFLAGS += '-DKEYRINGFILE="$(SYSCONFPREFIX)/keyring"'
CFLAGS += '-DEKEYD_VERSION_S=""$(EKEYD_VERSION_S)""'
CFLAGS += $(EXTRA_CFLAGS)

LIBS += -llua$(LUA_V) -lm $(LIBDL)
LDFLAGS += $(LIBDIRS)


all: all-programs all-scripts all-configs

install: all install-ekeyd

all-programs: ekeyd ekey-setkey

ifneq ($(BUILD_ULUSBD),no)
all-programs: ekey-ulusbd
install: install-ekey-ulusbd
endif

ifneq ($(BUILD_EGDLINUX),no)
all-programs: egd-linux
install: install-egd-linux
endif

all-scripts: ekey-rekey ekeydctl

all-configs: ekeyd.conf

ekey-ulusbd: ekey-ulusbd.o daemonise_ulusbd.o
	$(CC) $(CFLAGS) $(PTHFLAGS) $(LDFLAGS) -o $@ $^ -lusb $(PTHLIBS)

ekey-ulusbd.o: ekey-ulusbd.c
	$(COMPILE.c) $(OUTPUT_OPTION) $(PTHFLAGS) $^

daemonise_ulusbd.o: daemonise.c
	$(COMPILE.c) $(OUTPUT_OPTION) $(PTHFLAGS) $^

egd-linux: egd-linux.o daemonise.o
	$(CC) $(CFLAGS) -o $@ $^

ekeyd: ekeyd.o daemonise.o lstate.o connection.o stream.o frame.o packet.o keydb.o util.o fds.o krnlop.o foldback.o stats.o nonce.o ../device/frames/pem.o ../device/skeinwrap.o ../device/skein/skein.o ../device/skein/skein_block.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

ekey-setkey: ekey-setkey.o util.o stream.o frame.o packet.o keydb.o crc8.o nonce.o ../device/frames/pem.o ../device/skeinwrap.o ../device/skein/skein.o ../device/skein/skein_block.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

control.inc: bin2c.lua control.lua
	lua$(LUA_V) bin2c.lua +control.lua result > control.inc.new
	mv control.inc.new control.inc

lstate.o: lstate.c control.inc

%: %.in
	sed -e's|@PREFIX@|$(PREFIX)|g' -e's|@SYSCONFPREFIX@|$(SYSCONFPREFIX)|g' -e's|@RUNTIMEPREFIX@|$(RUNTIMEPREFIX)|g' -e's|@LUA_V@|$(LUA_V)|g' -e's|@KERNOUTOK@|$(KERNOUTOK)|' -e's|@KERNOUTNOTOK@|$(KERNOUTNOTOK)|' -e's|@EGDSOCK@|$(EGDSOCK)|' -e's|@UDEVOK@|$(UDEVOK)|' -e's|@EKEYD_VERSION_S@|$(EKEYD_VERSION_S)|' < $< > $@
	if test -x $<; then chmod +x $@; fi

install-ekey-ulusbd:
	mkdir -p $(DESTDIR)$(PREFIX)/sbin
	install -m 755 ekey-ulusbd $(DESTDIR)$(PREFIX)/sbin/
	mkdir -p $(DESTDIR)$(MANPREFIX)8
	$(MANZCMD) < ekey-ulusbd.8 > $(DESTDIR)$(MANPREFIX)8/ekey-ulusbd.8$(MANZEXT)

install-egd-linux:
	mkdir -p $(DESTDIR)$(PREFIX)/sbin
	install -m 755 egd-linux $(DESTDIR)$(PREFIX)/sbin/
	mkdir -p $(DESTDIR)$(MANPREFIX)8
	$(MANZCMD) < egd-linux.8 > $(DESTDIR)$(MANPREFIX)8/egd-linux.8$(MANZEXT)

install-ekeyd:
	mkdir -p $(DESTDIR)$(PREFIX)/sbin
	install -m 755 ekeyd $(DESTDIR)$(PREFIX)/sbin/
	install -m 755 ekey-setkey $(DESTDIR)$(PREFIX)/sbin/
	install -m 755 ekey-rekey $(DESTDIR)$(PREFIX)/sbin/
	install -m 755 ekeydctl $(DESTDIR)$(PREFIX)/sbin/
	mkdir -p $(DESTDIR)$(MANPREFIX)8
	$(MANZCMD) < ekeyd.8 > $(DESTDIR)$(MANPREFIX)8/ekeyd.8$(MANZEXT)
	$(MANZCMD) < ekey-setkey.8 > $(DESTDIR)$(MANPREFIX)8/ekey-setkey.8$(MANZEXT)
	$(MANZCMD) < ekey-rekey.8 > $(DESTDIR)$(MANPREFIX)8/ekey-rekey.8$(MANZEXT)
	$(MANZCMD) < ekeydctl.8 > $(DESTDIR)$(MANPREFIX)8/ekeydctl.8$(MANZEXT)
	mkdir -p $(DESTDIR)$(MANPREFIX)5
	$(MANZCMD) < ekeyd.conf.5 > $(DESTDIR)$(MANPREFIX)5/ekeyd.conf.5$(MANZEXT)
	mkdir -p $(DESTDIR)$(SYSCONFPREFIX)
	install -m 644 ekeyd.conf $(DESTDIR)$(SYSCONFPREFIX)
	echo "# Keyring Data file. Managed by ekey-setkey" > $(DESTDIR)$(SYSCONFPREFIX)/keyring
	chmod 0600 $(DESTDIR)$(SYSCONFPREFIX)/keyring

clean:
	$(RM) rdpkt ekeyd ekey-setkey *.o control.inc ../device/skeinwrap.o ../device/frames/pem.o ../device/skein/skein.o ../device/skein/skein_block.o ekeyd.conf ekey-rekey egd-linux control.inc.new ekeydctl ekey-ulusbd *.gcda gmon.out

olddeps:
	sudo apt-get install lua5.1 liblua5.1-socket2 liblua5.1-posix0 liblua5.1-dev libusb-dev

deps:
	sudo apt-get install lua5.1 liblua5.1-socket2 liblua5.1-posix1 liblua5.1-dev libusb-dev
