/*
  HradecFS
  Copyright (C) 2018 Roberto Hradec <me@hradec.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer".

*/



#include "params.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "log.h"

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <cstdarg>
#include <thread>
#include <time.h>
#include <boost/algorithm/string/replace.hpp>


#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <glob.h>
#include <pthread.h>
#include <stdbool.h>
#include <glob.h>

#include <utime.h>
//
#include <sys/types.h>
#include <sys/syscall.h>



// #include "cache_utils.h"

using namespace std;

#define u_int8 unsigned char
#define BUF_SIZE 2048

// FILE * log_open_UDP(int port)
// {
//   u_int8 *buf;
//   int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
//   struct sockaddr_in clientAddr;
//   if (sockfd < 0) {
//     printf("Could not open socket\n");
//     exit(1);
//   }
//   memset((char *)&clientAddr, 0, sizeof(clientAddr));
//   // buf = malloc(BUF_SIZE * sizeof(u_int8));
//   clientAddr.sin_family = AF_INET;
//   clientAddr.sin_port = htons(port);
//   if (inet_aton("127.0.0.1", &clientAddr.sin_addr)==0) {
//     printf("inet_aton()\n");
//     abort();
//   }
//   int rc = bind(sockfd, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
//   FILE *sockfile;
//   sockfile = (FILE*)fdopen(sockfd, "w");
//   return sockfile;
// }




//missing string printf
//this is safe and convenient but not exactly efficient
// inline std::string _format(const char* fmt, ...){
//     int size = 4096;
//     char* buffer = 0;
//     buffer = new char[size];
//     va_list vl;
//     va_start(vl, fmt);
//     int nsize = vsnprintf(buffer, size, fmt, vl);
//     if(size<=nsize){ //fail delete buffer and try again
//         delete[] buffer;
//         buffer = 0;
//         buffer = new char[nsize+1]; //+1 for /0
//         nsize = vsnprintf(buffer, size, fmt, vl);
//     }
//     std::string ret(buffer);
//     va_end(vl);
//     delete[] buffer;
//     return ret;
// }


FILE *log_open()
{
    FILE *logfile;

    // very first thing, open up the logfile and mark that we got in
    // here.  If we can't open the logfile, we're dead.
    logfile = fopen("bbfs.log", "w");
    if (logfile == NULL) {
	perror("logfile");
	exit(EXIT_FAILURE);
    }

    // set logfile to line buffering
    setvbuf(logfile, NULL, _IOLBF, 0);

    return logfile;
}

FILE *log_open_pipe(int flag)
{
    FILE *logfile;

    // very first thing, open up the logfile and mark that we got in
    // here.  If we can't open the logfile, we're dead.
    // logfile = fopen("bbfs.log", "w");

    if( flag == 0 ){
        logfile == NULL;
    }else{

        mkfifo("/tmp/.bbfs.log", 0666);
        int fifo_fd = open("/tmp/.bbfs.log",  O_RDWR | O_NONBLOCK);
        printf ("....%d....", fifo_fd);
        logfile = fdopen(fifo_fd, "w");

        if (logfile == NULL) {
        	perror("logfile");
        	exit(EXIT_FAILURE);
        }

        // set logfile to line buffering
        setvbuf(logfile, NULL, _IOLBF, 0);

    }


    return logfile;
}

void log_msg(const char *format, ...)
{
    va_list ap;
    char buff[8192];
    char buff2[8192];
    va_start(ap, format);
    vsprintf(buff, format,  ap);
    va_end (ap);

    if( ! fuse_get_context() )
        return;

    if ( BB_DATA->log && BB_DATA->logfile != NULL ){


        string reset    ="\033[0m";
        string green    ="\033[0;32m";
        string bgreen   ="\033[1;32m";
        string red      ="\033[0;31m";
        string bred     ="\033[1;31m";
        string yellow   ="\033[0;33m";
        string byellow  ="\033[1;33m";
        string blue     ="\033[0;34m";
        string bblue    ="\033[1;34m";
        string magenta  ="\033[0;35m";
        string bmagenta ="\033[1;35m";
        string cyan     ="\033[0;36m";
        string bcyan    ="\033[1;36m";



        // va_start(ap, format);
        // vfprintf(BB_DATA->logfile, format,  ap);
        // va_end (ap);

        // string tmp = string("\nhradecFS: ") + format; //_format( , ap);
        string tmp;
        tmp = boost::replace_all_copy( string(buff), "\n\n\n", "\n\n" );
        if( tmp.find("REMOTE") != string::npos ){
            tmp = bcyan+"=======================================\n"+tmp;
        }
        tmp = boost::replace_all_copy( tmp, "\n\n", "\n" );

        sprintf( buff2, "\nthread(%d) | ", syscall(__NR_gettid) );
        tmp = boost::replace_all_copy( tmp, "\n", byellow + buff2 + "hradecFS:" );
        tmp = boost::replace_all_copy( tmp, "hradecFS: hradecFS:", green+"hradecFS:"+bgreen+"=======================================  "+reset );
        tmp = boost::replace_all_copy( tmp, "hradecFS:", green+"hradecFS:"+reset );
        tmp = boost::replace_all_copy( tmp, "REMOTE", bred+"REMOTE" );
        tmp = boost::replace_all_copy( tmp, "!!!", bred+"!!!" );
        tmp = boost::replace_all_copy( tmp, "exist", yellow+"exist" );
        tmp = boost::replace_all_copy( tmp, ">>>", bblue+">>>" );
        tmp = boost::replace_all_copy( tmp, "===", cyan+"===" );
        tmp = boost::replace_all_copy( tmp, "---", bcyan+"---" );
        tmp = boost::replace_all_copy( tmp, "1.doCache", bmagenta+"1.doCache" );
        tmp = boost::replace_all_copy( tmp, "\n", reset+"\n" );


        fprintf( BB_DATA->logfile, tmp.c_str() );

        // fprintf( stderr,  tmp.c_str());
        fflush(BB_DATA->logfile);
    }
}

// Report errors to logfile and give -errno to caller
int log_error(const char *func)
{
    int ret = -errno;

    log_msg("    ERROR %s: %s\n", func, strerror(errno));

    return ret;
}

// fuse context
void log_fuse_context(struct fuse_context *context)
{
    log_msg("    context:\n");

    /** Pointer to the fuse object */
    //	struct fuse *fuse;
    log_struct(context, fuse, %08x, );

    /** User ID of the calling process */
    //	uid_t uid;
    log_struct(context, uid, %d, );

    /** Group ID of the calling process */
    //	gid_t gid;
    log_struct(context, gid, %d, );

    /** Thread ID of the calling process */
    //	pid_t pid;
    log_struct(context, pid, %d, );

    /** Private filesystem data */
    //	void *private_data;
    log_struct(context, private_data, %08x, );
    log_struct(((struct hradecFS_state *)context->private_data), logfile, %08x, );
    log_struct(((struct hradecFS_state *)context->private_data), rootdir, %s, );

    /** Umask of the calling process (introduced in version 2.8) */
    //	mode_t umask;
    log_struct(context, umask, %05o, );
}

// struct fuse_conn_info contains information about the socket
// connection being used.  I don't actually use any of this
// information in bbfs
void log_conn(struct fuse_conn_info *conn)
{
    log_msg("    conn:\n");

    /** Major version of the protocol (read-only) */
    // unsigned proto_major;
    log_struct(conn, proto_major, %d, );

    /** Minor version of the protocol (read-only) */
    // unsigned proto_minor;
    log_struct(conn, proto_minor, %d, );

    /** Is asynchronous read supported (read-write) */
    // unsigned async_read;
    // log_struct(conn, async_read, %d, );

    /** Maximum size of the write buffer */
    // unsigned max_write;
    log_struct(conn, max_write, %d, );

    /** Maximum readahead */
    // unsigned max_readahead;
    log_struct(conn, max_readahead, %d, );

    /** Capability flags, that the kernel supports */
    // unsigned capable;
    log_struct(conn, capable, %08x, );

    /** Capability flags, that the filesystem wants to enable */
    // unsigned want;
    log_struct(conn, want, %08x, );

    /** Maximum number of backgrounded requests */
    // unsigned max_background;
    log_struct(conn, max_background, %d, );

    /** Kernel congestion threshold parameter */
    // unsigned congestion_threshold;
    log_struct(conn, congestion_threshold, %d, );

    /** For future use. */
    // unsigned reserved[23];
}

// struct fuse_file_info keeps information about files (surprise!).
// This dumps all the information in a struct fuse_file_info.  The struct
// definition, and comments, come from /usr/include/fuse/fuse_common.h
// Duplicated here for convenience.
void log_fi (struct fuse_file_info *fi)
{
    return;
    log_msg("    fi:\n");

    /** Open flags.  Available in open() and release() */
    //	int flags;
	log_struct(fi, flags, 0x%08x, );

    /** Old file handle, don't use */
    //	unsigned long fh_old;
	// log_struct(fi, fh_old, 0x%08lx,  );

    /** In case of a write operation indicates if this was caused by a
        writepage */
    //	int writepage;
	log_struct(fi, writepage, %d, );

    /** Can be filled in by open, to use direct I/O on this file.
        Introduced in version 2.4 */
    //	unsigned int keep_cache : 1;
	log_struct(fi, direct_io, %d, );

    /** Can be filled in by open, to indicate, that cached file data
        need not be invalidated.  Introduced in version 2.4 */
    //	unsigned int flush : 1;
	log_struct(fi, keep_cache, %d, );

    /** Padding.  Do not use*/
    //	unsigned int padding : 29;

    /** File handle.  May be filled in by filesystem in open().
        Available in all other file operations */
    //	uint64_t fh;
	log_struct(fi, fh, 0x%016llx,  );

    /** Lock owner id.  Available in locking operations and flush */
    //  uint64_t lock_owner;
	log_struct(fi, lock_owner, 0x%016llx, );
}

void log_retstat(const char *func, int retstat)
{
    int errsave = errno;
    log_msg("    %s returned %d\n", func, retstat);
    errno = errsave;
}

// make a system call, checking (and reporting) return status and
// possibly logging error
int log_syscall(const char *func, int retstat, int min_ret)
{
    log_retstat(func, retstat);

    if (retstat < min_ret) {
	log_error(func);
	retstat = -errno;
    }

    return retstat;
}

// This dumps the info from a struct stat.  The struct is defined in
// <bits/stat.h>; this is indirectly included from <fcntl.h>
void log_stat(struct stat *si)
{
    log_msg("    si:\n");

    //  dev_t     st_dev;     /* ID of device containing file */
	log_struct(si, st_dev, %lld, );

    //  ino_t     st_ino;     /* inode number */
	log_struct(si, st_ino, %lld, );

    //  mode_t    st_mode;    /* protection */
	log_struct(si, st_mode, 0%o, );

    //  nlink_t   st_nlink;   /* number of hard links */
	log_struct(si, st_nlink, %d, );

    //  uid_t     st_uid;     /* user ID of owner */
	log_struct(si, st_uid, %d, );

    //  gid_t     st_gid;     /* group ID of owner */
	log_struct(si, st_gid, %d, );

    //  dev_t     st_rdev;    /* device ID (if special file) */
	log_struct(si, st_rdev, %lld,  );

    //  off_t     st_size;    /* total size, in bytes */
	log_struct(si, st_size, %lld,  );

    //  blksize_t st_blksize; /* blocksize for filesystem I/O */
	log_struct(si, st_blksize, %ld,  );

    //  blkcnt_t  st_blocks;  /* number of blocks allocated */
	log_struct(si, st_blocks, %lld,  );

    //  time_t    st_atime;   /* time of last access */
	log_struct(si, st_atime, 0x%08lx, );

    //  time_t    st_mtime;   /* time of last modification */
	log_struct(si, st_mtime, 0x%08lx, );

    //  time_t    st_ctime;   /* time of last status change */
	log_struct(si, st_ctime, 0x%08lx, );

}

void log_statvfs(struct statvfs *sv)
{
    log_msg("    sv:\n");

    //  unsigned long  f_bsize;    /* file system block size */
	log_struct(sv, f_bsize, %ld, );

    //  unsigned long  f_frsize;   /* fragment size */
	log_struct(sv, f_frsize, %ld, );

    //  fsblkcnt_t     f_blocks;   /* size of fs in f_frsize units */
	log_struct(sv, f_blocks, %lld, );

    //  fsblkcnt_t     f_bfree;    /* # free blocks */
	log_struct(sv, f_bfree, %lld, );

    //  fsblkcnt_t     f_bavail;   /* # free blocks for non-root */
	log_struct(sv, f_bavail, %lld, );

    //  fsfilcnt_t     f_files;    /* # inodes */
	log_struct(sv, f_files, %lld, );

    //  fsfilcnt_t     f_ffree;    /* # free inodes */
	log_struct(sv, f_ffree, %lld, );

    //  fsfilcnt_t     f_favail;   /* # free inodes for non-root */
	log_struct(sv, f_favail, %lld, );

    //  unsigned long  f_fsid;     /* file system ID */
	log_struct(sv, f_fsid, %ld, );

    //  unsigned long  f_flag;     /* mount flags */
	log_struct(sv, f_flag, 0x%08lx, );

    //  unsigned long  f_namemax;  /* maximum filename length */
	log_struct(sv, f_namemax, %ld, );

}

void log_utime(struct utimbuf *buf)
{
    log_msg("    buf:\n");

    //    time_t actime;
    log_struct(buf, actime, 0x%08lx, );

    //    time_t modtime;
    log_struct(buf, modtime, 0x%08lx, );
}
