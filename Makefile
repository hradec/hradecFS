#
# HradecFS
# Copyright (C) 2018 Roberto Hradec <me@hradec.com>
#
# This program can be distributed under the terms of the GNU GPLv3.
# See the file COPYING.
#
# HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer".
#




AWK = gawk
CC = g++
# CCDEPMODE = depmode=gcc3
CFLAGS = -g -O2
# CFLAGS = -g
CPP = g++ -E
LINK=g++
CPPFLAGS =
CYGPATH_W = echo
# DEFS = -DHAVE_CONFIG_H
DEPDIR = .deps
ECHO_C =
ECHO_N = -n
ECHO_T =
EGREP = /usr/sbin/grep -E
EXEEXT =
FUSE_CFLAGS = -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -fpermissive
FUSE_LIBS = -lfuse -pthread
GREP = /usr/sbin/grep
LDFLAGS =
LIBOBJS =
LIBS =
LTLIBOBJS =
MKDIR_P = /usr/sbin/mkdir -p
OBJEXT = o
PACKAGE = fuse-tutorial
SHELL = /bin/sh


OBJECTS=$(shell ls *.c | sed 's/\.c/.o /g')
# $(info $(bbfs_OBJECTS))
FUSE_LIBS =  -lfuse -pthread




COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	$(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)


.SUFFIXES:
.SUFFIXES: .c .o .obj


hradecFS$(EXEEXT): $(OBJECTS) $(shell ls *.h)
	[ "$$(mount | grep hradecFS)" != "" ] && sudo umount $$(mount | grep hradecFS | awk '{print $$3}') || true
	rm -f hradecFS$(EXEEXT)
	$(LINK) $(OBJECTS) $(FUSE_LIBS) $(LIBS) -o $@


all: hradecFS


TEST_FS=/ZRAID2

test: all
	$(MKDIR_P) /tmp/xx
	[ "$$(mount | grep hradecFS)" != "" ] && sudo umount $$(mount | grep hradecFS | awk '{print $$3}') || true
	./hradecFS -o nonempty $(TEST_FS)/ /tmp/xx
	echo "Folder $(TEST_FS) mounted on /tmp/xx!!"


.c.o:
	$(COMPILE) -MT $@ -MD -MP $(FUSE_CFLAGS) $(CFLAGS) -c -o $@ $<
	# $(COMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tpo -c -o $@ $<
	# $(AM_V_at)$(am__mv) $(DEPDIR)/$*.Tpo $(DEPDIR)/$*.Po
	# $(AM_V_CC)source='$<' object='$@' libtool=no \
	# DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) \
	# $(AM_V_CC_no)$(COMPILE) -c -o $@ $<


clean:
	@rm -fv *.o
	@rm -fv *.d
	@rm -fv hradecFS
	@rm -fv a.out
