/*
  HradecFS
  Copyright (C) 2018 Roberto Hradec <me@hradec.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer".

*/

#include "fuse_setup.h"

#ifndef  __FILE_UTILS_H__
#define  __FILE_UTILS_H__

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <glob.h>
#include <pthread.h>
#include <stdbool.h>
#include <glob.h>

#include "log.h"


#include <glob.h>
#include <vector>
#include <string>


#include <sys/types.h>
#include <utime.h>


static pthread_mutex_t mutex_umask;
mode_t getUmask(void){
    mode_t _umask;
    pthread_mutex_lock(&mutex_umask);
    _umask = umask(0);
    umask( _umask );
    pthread_mutex_unlock(&mutex_umask);
    return _umask;
}


inline std::vector<std::string> glob(const std::string& pat, const char* caller = __builtin_FUNCTION()){
    using namespace std;
    glob_t glob_result;
    int retr = glob(pat.c_str(),GLOB_TILDE,NULL,&glob_result);
    log_msg( "caller(%s) -> glob( %s ) = %d - return %d\n", caller,  pat.c_str(), (unsigned int)glob_result.gl_pathc, retr );
    vector<string> ret;
    for(unsigned int i=0;i<glob_result.gl_pathc;++i){
        ret.push_back(string(glob_result.gl_pathv[i]));
        // log_msg("\t%s \n", string(glob_result.gl_pathv[i]).c_str() );
    }
    globfree(&glob_result);
    return ret;
}



char existsSymLink(const char *path)
{
  struct stat s;
  int err = stat(path, &s);
  if( -1 == err )
    return 0;
  return 1;
}



char islnk(const char *path)
{
  struct stat s;
  int err = lstat(path, &s);
  if(-1 == err || ! S_ISLNK(s.st_mode)) {
    return 0;
  }
  return 1;
}


char exists(const char *path, const char* caller = __builtin_FUNCTION())
{
  if( islnk( path ) ){
      log_msg( "\nexists is link = true - called from caller(%s)\n", caller );
      return 1;
  }
  struct stat s;
  int err = stat(path, &s);
  if( -1 == err )
    err = lstat(path, &s);
    if( -1 == err )
        return 0;
  return 1;
}

 char * getFileCreationTime(char *buff, char *filePath)
 {
     struct stat attrib;
     stat(filePath, &attrib);
     char date[10];
     strftime(date, 10, "%d-%m-%y", gmtime(&(attrib.st_ctime)));
     sprintf(buff, "The file %s was last modified at %s\n", filePath, date);
     date[0] = 0;
     return buff;
 }


char chown(string path, uid_t owner, gid_t group){
    return chown(path.c_str(), owner, group);
}


char isdir(string path, const char* caller = __builtin_FUNCTION()){
    return isdir(path.c_str());
}
char isfile(string path, const char* caller = __builtin_FUNCTION()){
    return isfile(path.c_str());
}
char islnk(string path, const char* caller = __builtin_FUNCTION()){
    return islnk(path.c_str());
}

char isdir(const char *path, const char* caller = __builtin_FUNCTION())
{
  if( islnk( path ) ){
    return 0;
  }
  struct stat s;
  int err = stat(path, &s);
  if(-1 == err || ! S_ISDIR(s.st_mode)) {
    return 0;
  }
  return 1;
}
char isfile(const char *path)
{
  if( islnk( path ) ){
    return 0;
  }
  struct stat s;
  int err = stat(path, &s);
  if(-1 == err || ! S_ISREG(s.st_mode)) {
    return 0;
  }
  return 1;
}
char *replace_str(char *str, const char *orig, const char *rep, const char* caller = __builtin_FUNCTION())
{
   static char buffer[1024];
   char *p;
   int i = 0;

   if (!(p = strstr(str + i, orig)))
   {
       return str;
   }

   while (str[i])
   {
       if (!(p = strstr(str + i, orig)))
       {
           strcat(buffer, str + i);
           break; //return str;
       }
       strncpy(buffer + strlen(buffer), str + i, (p - str) - i);
       buffer[p - str] = '\0';
       strcat(buffer, rep);
       //printf("STR:%s\n", buffer);
       i = (p - str) + strlen(orig);
   }

   return buffer;
}

long getFileSize(std::string filename,const char* caller = __builtin_FUNCTION())
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

bool checkFileSize(const char *path1, const char *path2, const char* caller = __builtin_FUNCTION())
{
    // get size
    struct stat fpath_st;
    stat(path1, &fpath_st);

    // get size
    struct stat cachefile_st;
    stat(path2, &cachefile_st);

    log_msg( "\ncheckFileSize: %s sizeRemote: %lld ", path1, (long long) fpath_st.st_size  );
    log_msg( "\ncheckFileSize: %s sizeRemote: %lld \n", path2, (long long) cachefile_st.st_size  );

    // check if the 2 sizes are the same or not!
    return fpath_st.st_size == cachefile_st.st_size;
}

int replace(char *str, char orig, char rep) {
    char *ix = str;
    int n = 0;
    while((ix = strchr(ix, orig)) != NULL) {
        *ix++ = rep;
        n++;
    }
    return n;
}


void __stat__(const char *fpath, struct stat *statbuf, char *cacheFile, const char* caller = __builtin_FUNCTION()){
    char buf[1024];
    unsigned long len;
    // log_stat(statbuf);
    if(islnk(fpath)){

        if ((len = readlink(fpath, buf, sizeof(buf)-1)) != -1){
            buf[len] = '\0';
            log_msg("\ncaller(%s) - link %s = %s\n", caller, fpath, buf);
            symlink(buf, cacheFile);
        }
    }else if(isfile(fpath)){
        creat(cacheFile, statbuf->st_mode);
    }else if( isdir(fpath) ){
        mkdir(cacheFile, statbuf->st_mode);
    }else{
        mkdir(cacheFile, statbuf->st_mode);
        // symlink(const char *pathname, const char *slink);
    }
    if( exists(cacheFile) ){
        chown(cacheFile, statbuf->st_uid, statbuf->st_gid);
        // log_msg("\nhradecFS_getattr_cached(path=\"%s\", statbuf=0x%08x)\n",	  CACHE_FILE, statbuf);
    }
}


long checkForRsyncTemp(char *path, const char* caller = __builtin_FUNCTION()){
  // check if it's already caching
  glob_t        globlist;
  int           ret;
  ret = glob(replace_str(path, "_locked",".*"), GLOB_PERIOD, NULL, &globlist);

  // if temp file exists...
  if ( ret == 0 ) {
    struct stat cachefile_st;
    stat(globlist.gl_pathv[0], &cachefile_st);
    return cachefile_st.st_size;
  } else {
    return -1;
  }
}


void waitForLock(char *path, const char* caller = __builtin_FUNCTION()){
  long cur_size=-1;
  long old_size=-2;
  char count=0;
  while ( exists( path ) ) {
    sleep(1);
    // check for the size of the rsync temp file
    cur_size = checkForRsyncTemp( path );
    // if size of rsync temp file is bigger than old_size, reset counter
    // and start monitoring the size of the file being transmitted
    // it should keep increasing!!
    if ( cur_size > old_size ){
      count = 0;
      old_size = cur_size;
    }else{
      // if the size of the file stalls or file doesn't exists
      // start counting!!!
      count++;
    }

    // if count > 60 segundos, exit and CAUSE file error!!!
    if ( count > 60 ){
      break;
    }

  }

}


static void hradecFS_fullpath(char fpath[PATH_MAX], const char *path)
{
    strcpy(fpath, BB_DATA->rootdir);
    strncat(fpath, path, PATH_MAX); // ridiculously long paths will
                    // break here

    log_msg("    hradecFS_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",  BB_DATA->rootdir, path, fpath);
}


bool grep( const char *file, const char *str ){
    #define MAXBUFLEN 1000000
    bool ret=false;
    char source[MAXBUFLEN + 1];
    FILE *fp = fopen(file, "r");
    if (fp != NULL) {
        while(!feof(fp)){
            size_t newLen = fread(source, sizeof(char), MAXBUFLEN, fp);
            if ( ferror( fp ) != 0 ) {
                fputs("Error reading file", stderr);
                break;
            } else {
                if( strstr(source, str) != NULL ){
                    ret=true;
                    break;
                }
            }
        }
        fclose(fp);
    }
    return ret;

}


#endif
