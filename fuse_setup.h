/*
  HradecFS
  Copyright (C) 2018 Roberto Hradec <me@hradec.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer".

*/


#ifndef FUSE_USE_VERSION

#define FUSE_USE_VERSION 32

// we need this for touch to work (and also to properly work with NFS)
// this fixes SETATTR "function not implemented" error!
#define HAVE_UTIMENSAT

#include <fuse3/fuse.h>
#include <fuse3/fuse_lowlevel.h>


#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#endif
