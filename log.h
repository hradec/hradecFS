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
#include "params.h"

#include <stdio.h>

#include <string>
using namespace std;


// log_msgN is a binary on/off. to enable each one, just assign
// a chr number with the correspondent bit ON.
// ex: to enable log_msg and log_msg5, do:
//       echo 17 > /<hradecFS>/.logon
// since 17 is 00010001 in binary (first and fifth bit on!). all others will be off

#define log_msg         if( fuse_get_context() && (BB_DATA->log&1  ) )  __log_msg
#define log_msg2        if( fuse_get_context() && (BB_DATA->log&2  ) )  __log_msg
#define log_msg4        if( fuse_get_context() && (BB_DATA->log&8  ) )  __log_msg
#define log_msg3        if( fuse_get_context() && (BB_DATA->log&4  ) )  __log_msg
#define log_msg5        if( fuse_get_context() && (BB_DATA->log&16 ) )  __log_msg
#define log_msg6        if( fuse_get_context() && (BB_DATA->log&32 ) )  __log_msg
#define log_msg7        if( fuse_get_context() && (BB_DATA->log&64 ) )  __log_msg
#define log_msg8        if( fuse_get_context() && (BB_DATA->log&128) )  __log_msg



//  macro to log fields in structs.
#define log_struct(st, field, format, typecast) \
  log_msg("    " #field " = " #format "\n", typecast st->field)

FILE *log_open(void);
FILE *log_open_pipe(int flag=1);
bool __log_msg(const char *format, ...);
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
