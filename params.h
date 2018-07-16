/*
  HradecFS
  Copyright (C) 2018 Roberto Hradec <me@hradec.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer".

*/

#ifndef _PARAMS_H_
#define _PARAMS_H_


// need this to get pwrite().  I have to use setvbuf() instead of
// setlinebuf() later in consequence.
// #define _XOPEN_SOURCE 500

// maintain bbfs state in here
#include <limits.h>
#include <stdio.h>
struct hradecFS_state {
    FILE *logfile;
    char *rootdir;
    char *mountdir;
    char *cachedir;
    char *syncCommand;
    int log;
};
#define FUSE_CTX fuse_get_context()
#define BB_DATA ((struct hradecFS_state *)FUSE_CTX->private_data)

#endif
