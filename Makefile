#
# HradecFS
# Copyright (C) 2018 Roberto Hradec <me@hradec.com>
#
# This program can be distributed under the terms of the GNU GPLv3.
# See the file COPYING.
#
# HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer".
#


# CCDEPMODE 	= depmode=gcc3
# CFLAGS 		= -g -O3
CFLAGS 		= -g $(shell pkg-config fuse --cflags --libs)
AWK 		= gawk
CC 			= g++
CPP 		= g++ -E
LINK		= g++
CYGPATH_W 	= echo
DEPDIR 		= .deps
ECHO_N 		= -n
EGREP 		= /usr/sbin/grep -E
FUSE_CFLAGS = -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -fpermissive
FUSE_LIBS 	= -lfuse -pthread
GREP      	= /usr/sbin/grep
EXEEXT 		=
LDFLAGS   	=
LIBOBJS   	=
LIBS      	=
LTLIBOBJS 	=
MKDIR_P   	= /usr/sbin/mkdir -p
OBJEXT 	  	= o
SHELL 	  	= /bin/sh
FUSE_LIBS 	= -lfuse3 -pthread
OBJECTS	  	= $(shell ls *.c | sed 's/\.c/.o /g')
DEPS	  	= $(shell ls -1 *.h)
PWD			= $(shell pwd)

COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)

TEST_FS=/ZRAID2/

.SUFFIXES: .c .o .obj


all: hradecFS

hradecFS$(EXEEXT): $(OBJECTS)
	rm -f hradecFS$(EXEEXT)
	$(LINK) $(OBJECTS) $(FUSE_LIBS) $(LIBS) -o $@



docker:
	docker run -ti --rm \
		--device /dev/fuse \
		-v /etc/passwd:/etc/passwd \
		-v /etc/groups:/etc/groups \
		-v /ssd/atomo_cachedir:/atomo_cachedir \
		-v $(PWD):/hradecFS \
		-v $(HOME):$(HOME) \
		--cap-add SYS_ADMIN \
		--name hradecFS hradec/docker_lizardfs \
		/bin/bash -c "\
			yaourt -Sy sshfs --noconfirm ; cd /hradecFS ; mkdir /r610 ; chmod a+rw /r610 ; \
			mkdir /root/.ssh ;  echo 'StrictHostKeyChecking no' > /root/.ssh/config ; \
			sshfs -o allow_other -o IdentityFile=$(HOME)/.ssh/id_rsa $(USER)@192.168.0.12:/ZRAID/ /r610 ; \
			/hradecFS/hradecFS  -o allow_other  /r610/atomo/ /atomo/; \
			runuser - rhradec -c '/bin/bash --init-file /atomo/pipeline/tools/init/bash' \
		"
log:
	while true ; do docker exec -ti hradecFS cat /tmp/.bbfs.log ; done

test: all cleanTest upload
	$(MKDIR_P) /tmp/xx
	sudo su - -c "$(shell pwd)/hradecFS -o allow_other $(TEST_FS)/ /tmp/xx"
	@echo "Folder $(TEST_FS) mounted on /tmp/xx!!"


debug: all cleanTest upload
	$(MKDIR_P) /tmp/xx
	sudo su - -c "$(shell pwd)/hradecFS -d -o allow_other $(TEST_FS)/ /tmp/xx > /tmp/debug.log &"
	@echo "Folder $(TEST_FS) mounted on /tmp/xx!!"
	tail -f /tmp/debug.log


%.o: %.c $(DEPS)
	$(COMPILE) -MT $@ -MD -MP $(FUSE_CFLAGS) $(CFLAGS) -c -o $@ $<
	# $(COMPILE) -MT $@ -MD -MP -MF $(DEPDIR)/$*.Tpo -c -o $@ $<
	# $(AM_V_at)$(am__mv) $(DEPDIR)/$*.Tpo $(DEPDIR)/$*.Po
	# $(AM_V_CC)source='$<' object='$@' libtool=no \
	# DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) \
	# $(AM_V_CC_no)$(COMPILE) -c -o $@ $<


upload:
	#cp -rfv ./hradecFS /ZRAID2/
	gsutil cp ./hradecFS gs://zraid2/

cleanTest:
	[ "$$(mount | grep hradecFS)" != "" ] && sudo umount -f -l $$(mount | grep hradecFS | awk '{print $$3}') || true
	# sudo pkill -fc -9  "./hradecFS -o allow_other" || echo "killed hradecFS"
	sudo umount -f -l  /tmp/xx || echo "Not Mounted!"
	rm -rf /tmp/xx

cleanTestAll: cleanTest
	sudo rm -rf /tmp/xx_cachedir

clean: cleanTest
	rm -fv *.o
	rm -fv *.d
	rm -f hradecFS$(EXEEXT)
	rm -fv a.out

nuke: clean cleanTestAll
cleanAll: nuke
depclean: nuke
