/*
  HradecFS
  Copyright (C) 2018 Roberto Hradec <me@hradec.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer".

*/

#ifndef _LOG_H_
#define _LOG_H_

#include "fuse_setup.h"

#include <stdio.h>

#include <string>
using namespace std;


//  macro to log fields in structs.
#define log_struct(st, field, format, typecast) \
  log_msg("    " #field " = " #format "\n", typecast st->field)

FILE *log_open(void);
FILE *log_open_pipe(int flag=1);
void log_msg(const char *format, ...);
void log_conn(struct fuse_conn_info *conn);
int log_error(const char *func);
void log_fi(struct fuse_file_info *fi);
void log_fuse_context(struct fuse_context *context);
void log_retstat(const char *func, int retstat);
void log_stat(struct stat *si);
void log_statvfs(struct statvfs *sv);
int  log_syscall(const char *func, int retstat, int min_ret);
void log_utime(struct utimbuf *buf);

#endif
