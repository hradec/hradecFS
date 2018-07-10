/*
  HradecFS
  Copyright (C) 2018 Roberto Hradec <me@hradec.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  HradecFS is a simple and really fast cache file system for filesystem mounted over WAN!
  The biggest advantage of HradecFS is it a "folder oriented" cache, instead of "file oriented"
  cache filesystem, like all others out there.
  Being "folder oriented" means it caches a whole folder and all they
  files in it once ONE file in that folder is accessed.
  to make it reasonably fast when caching a folder for the first time, it actually
  caches an "skeleton" of every file, meaning all permissions, attributes and times.
  So no DATA of the files are cached. (only the requested file retrieves the file data, off course)
  This allows for searchpaths to work as fast as a local filesystem, since after
  the first time a folder is accessed, everything in it becomes available locally.
  For example, when PYTHONPATH defines paths in a remote filesystem. Python will search on
  the same folders in the pythonpath over and over again, for EVERY python file it
  needs to import.
  With all cache filesystems out there, each search for different files on the same folder
  will need a lookup on the remote filesystem, since THAT file wasn't cached yet.

  With HradecFS, every subsequent search on those folders will be done locally, so python
  becomes as fast as if the filesystem was local! (only slowing down when there's actually
  file data remotely that needs to be retrieved)

  HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer", which basically is
  an "example" code to create Fuse Filesystem.

  Below are the Copyright information for "Big Brother File System by Joseph J. Pfeiffer":

  Big Brother File System
  Copyright (C) 2012 Joseph J. Pfeiffer, Jr., Ph.D. <pfeiffer@cs.nmsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
  A copy of that code is included in the file fuse.h

  The point of this FUSE filesystem is to provide an introduction to
  FUSE.  It was my first FUSE filesystem as I got to know the
  software; hopefully, the comments in this code will help people who
  follow later to get a gentler introduction.

  This might be called a no-op filesystem:  it doesn't impose
  filesystem semantics on top of any other existing structure.  It
  simply reports the requests that come in, and passes them to an
  underlying filesystem.  The information is saved in a logfile named
  bbfs.log, in the directory from which you run bbfs.
*/


#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif


#include "cache.h"


extern "C" {

// MAIN cache class which handles all fuse functionality
__cache CACHE;
// __cache_setup_files CACHE_SETUP_FILES( CACHE );

void task1(void (*func)() ) {
    log_msg("\nmultithread --> task1\n");
    (*func)();
    sleep(60);
}
void cleanupCache(){
    CACHE.cleanupCache();
}

//===================================================================================================================
int hradecFS_getattr(const char *path, struct stat *statbuf, fuse_file_info *file_info)
{
    (void) file_info;
    int retstat=-1;
    char fpath[PATH_MAX];

    string spath = path;

    if( spath == "/.logoff" ) {
        log_msg("\n!!! log off\n");
        BB_DATA->log = false;
    }else if( spath == "/.logon" ) {
        BB_DATA->log = true;
        log_msg("\n!!! log on\n");
    }

    CACHE.init( path );

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_getattr %s [%d]\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);

    // if path exists remotely ( also returns true if the file exists locally only! )
    if ( CACHE.existsRemote( path ) ){

        // if we have the file cached (just a placeholder or the actual full file, doesn't matter)
        if( ! CACHE.existsLocal( path ) ){
            // CACHE.doCachePathParentDir( path );
            // CACHE.doCachePath( path, statbuf );
            log_msg("\nREMOTE  hradecFS_getattr_cache( CACHE.doCachePathParentDir(%s) )\n", CACHE.localPath( path ));
        }
    }else{
        // we need to return -2 to tell fuse the file doesn't exist!
        return -2;
    }

    retstat = CACHE.stat( path, statbuf );

    if( spath == "/.hradecFS_local_files" ){
        string p;
        int size=0;
        vector<string> files = glob(CACHE._cacheControl() + "*.__local__" );
        for ( unsigned int i = 0; i < files.size(); i++ ) {
            if( int( files[i].find(".hradecFS_") ) < 0 ){
                p = CACHE.getPathFromLogFile( files[i] );
                if( isfile( p.c_str() ) )
                    size += p.length()+1 ;
            }
        }
        statbuf->st_size = size;
    }

    log_msg("\n====>getattr   uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, retstat, path);
    return retstat;
}

//===================================================================================================================
int hradecFS_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_fgetattr  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    // log_msg("\nhradecFS_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n", path, statbuf, fi);
    // log_fi(fi);

    // On FreeBSD, trying to do anything with the mountpoint ends up
    // opening it, and then using the FD for an fgetattr.  So in the
    // special case of a path of "/", I need to do a getattr on the
    // underlying root directory instead of doing the fgetattr().
    if (!strcmp(path, "/"))
       return hradecFS_getattr(path, statbuf, fi);

    retstat = fstat(fi->fh, statbuf);
    if (retstat < 0)
        retstat = log_error("hradecFS_fgetattr fstat");

    log_stat(statbuf);

    log_msg("\n====>fgetattr   uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, retstat, path);
    return retstat;
}

//===================================================================================================================
int hradecFS_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int retstat = 0;
    char fpath[PATH_MAX];

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_opendir %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);

    CACHE.init( path );
    if ( ! CACHE.existsRemote( path ) ){
        return -1;
    }

    if( ! CACHE.isDirCached(path) ){
        // since opendir returns a pointer, takes some custom handling of
        // return status.
        //dp = opendir(CACHE.remotePath(path));
        CACHE.doCachePathParentDir( path );
        CACHE.doCachePathDir( path );
        // log_msg("CACHE.doCachePathDir(%s)   hradecFS_opendir(path=\"%s\", fi=0x%08x)\n",path, CACHE.remotePath(path), fi);
    }


    if( CACHE.existsLocal( path ) ){
        log_msg("\nopendir( CACHE.localPath(%s)=%s )\n",path, CACHE.localPath(path));
        dp = opendir(CACHE.localPath(path));
    }

    // log_msg("    opendir returned 0x%p\n", dp);
    if (dp == NULL)
       retstat = log_error("hradecFS_opendir opendir");

    fi->fh = (intptr_t) dp;

    log_msg("\n====>opendir   uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, retstat, path);
    return retstat;
}

//===================================================================================================================
int hradecFS_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
           struct fuse_file_info *fi,  fuse_readdir_flags flags)
{
    // CACHE DOC:
    //      We don't need to do anything here, since hradecFS_opendir is responsable for caching the
    //      folder and pass the correct patch to this function!
    //      all the caching mangling is done in hradecFS_opendir

    int retstat = 0;
    DIR *dp;
    struct dirent *de;

    string spath = path;

    CACHE.init( path );
    CACHE.doCachePathDir( path );
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_readdir  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);

    // log_msg("\nhradecFS_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",path, buf, filler, offset, fi);
    // once again, no need for fullpath -- but note that I need to cast fi->fh
    // dp = (DIR *) (uintptr_t) fi->fh;

    if( spath == "/" ){
        string pref = spath+".hradecFS_local_files";
        CACHE.init( pref );
        CACHE.setLocallyCreated( pref );
        close( creat( CACHE.localPath( pref ),  S_IRWXU | S_IRWXG | S_IRWXO  ) );
        CACHE.localFileExist( pref );
        // st.st_ino = -1;
        // filler( buf, ".hradecFS_local_files", &st, 0, 0 );
    }



    dp = opendir( CACHE.localPath( path ) );
    if (dp == NULL)
        return -errno;

    // Every directory contains at least two entries: . and ..  If my
    // first call to the system readdir() returns NULL I've got an
    // error; near as I can tell, that's the only condition under
    // which I can get an error from readdir()
    // de = readdir(dp);
    // // log_msg("    readdir returned 0x%p\n", de);
    // if (de == 0) {
    //     retstat = log_error("hradecFS_readdir readdir");
    //     return retstat;
    // }

    // This will copy the entire directory into the buffer.  The loop exits
    // when either the system readdir() returns NULL, or filler()
    // returns something non-zero.  The first case just means I've
    // read the whole directory; the second means the buffer is full.
    // do {
    struct stat st;
    while ((de = readdir(dp)) != NULL) {
        string entry = string(path)+"/"+de->d_name;
        CACHE.init(entry);

        log_msg("\033[1;31m calling filler with name %s - ino [%d]\n", entry.c_str(), de->d_ino);
        if( string(de->d_name)==".." || string(de->d_name)=="." || CACHE.existsRemote( entry ) ){
            memset(&st, 0, sizeof(st));
            st.st_ino = de->d_ino;
            st.st_mode = de->d_type << 12;
            // lstat( CACHE.localPath(entry), &st );
            if ( filler( buf, de->d_name, &st, 0, 0 ) ) {
                log_msg( "\033[1;31m      >>>>> ERROR hradecFS_readdir filler:  buffer full" );
                retstat = -ENOMEM;
                break;
            }
        }
    }
    closedir( dp );
    // log_fi(fi);

    log_msg("\n====>readdir   uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, retstat, path);
    return retstat;
}

//===================================================================================================================
int hradecFS_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_releasedir  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    // CACHE.init( path );
    // log_fi(fi);
    closedir((DIR *) (uintptr_t) fi->fh);

    log_msg("\n====>releasedir   uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, retstat, path);
    return retstat;
}

//===================================================================================================================
int hradecFS_readlink(const char *path, char *link, size_t size)
{
    int retstat;
    char fpath[PATH_MAX];

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_readlink  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );

    retstat = -1;
    // no path in the remote

    if ( ! CACHE.existsRemote( path ) ){
        return retstat;
        // CACHE.doCachePathParentDir( path );
        // CACHE.doCachePath( path );
    }

    if( CACHE.existsLocal( path ) ){
        retstat = log_syscall("CACHE.localPath", readlink(CACHE.localPath(path), link, size - 1), 0);
        log_msg("CACHE  hradecFS_readlink_cached(path=\"%s\", link=\"%s\", size=%d)\n", CACHE.localPath(path), link, size);
    // }else{
    //     if( ! CACHE.isDirCached(path) ) {
    //         log_msg("REMOTE  hradecFS_readlink(path=\"%s\", link=\"%s\", size=%d)\n", CACHE.remotePath(path), link, size);
    //         retstat = log_syscall("fpath", readlink(CACHE.remotePath(path), link, size - 1), 0);
    //     }
    }

    if (retstat >= 0) {
        link[retstat] = '\0';
        retstat = 0;
    }

    log_msg("\n====>readlink  uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, retstat, path);
    return retstat;
}

//===================================================================================================================
int hradecFS_mknod(const char *path, mode_t mode, dev_t dev)
{
    int retstat=-1;

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_mknod  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );




    // On Linux this could just be 'mknod(path, mode, dev)' but this
    // tries to be be more portable by honoring the quote in the Linux
    // mknod man page stating the only portable use of mknod() is to
    // make a fifo, but saying it should never actually be used for
    // that.
    CACHE.setLocallyCreated( path );

    // pthread_mutex_lock(&mutex);
    if (S_ISREG(mode)) {
        remove( CACHE.localPath( path ) );
        remove( CACHE.localPathLog( path ) );
        retstat = log_syscall("open", open( CACHE.localPath( path ), O_CREAT | O_EXCL | O_WRONLY, mode), 0);
        if (retstat >= 0){
            CACHE.localFileNotExistRemove( path );
            retstat = log_syscall("close", close(retstat), 0);
        }
    } else{
        if (S_ISFIFO(mode))
            retstat = log_syscall("mkfifo", mkfifo( CACHE.localPath( path ), mode ), 0);
        else
            retstat = log_syscall("mknod", mknod( CACHE.localPath( path ), mode, dev ), 0);

        if( retstat == 0 )
            CACHE.localFileNotExistRemove( path );
    }
    if( retstat >= 0 ){
        // create a log file for the newly created file!
        // we must add "100%" in it to fool openDir so
        // it WON'T try to pull it from remote!
        // string tmp = _format( "%s*%lld\n100%\n)", CACHE.localPath( path ) , 0 );
        //
        // pthread_mutex_lock(&__mutex_localFileExist);
        // int file = creat( CACHE.localPathLog( path ), mode );
        // write( file, tmp.c_str(),  tmp.length() );
        // close( file );
        // pthread_mutex_unlock(&__mutex_localFileExist);
        // CACHE.localFileNotExistRemove( path );
        CACHE.localFileExist( path );

    }
    // pthread_mutex_unlock(&mutex);
    log_msg("\n====>mknod     uid: [%4d]  mode: [0%3o] dev: [%lld]  return: [%2d] path: %s \n", fuse_get_context()->uid, mode, dev, retstat, path);

    return retstat;
}

//===================================================================================================================
int hradecFS_mkdir(const char *path, mode_t mode)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_mkdir  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);

    CACHE.init( path );
    log_msg("\nhradecFS_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);

    int ret = -1;
    if( ! CACHE.existsLocal( path ) ) {
        ret = 0;
        // if dir exists, don't try to create it again!
        if( ! exists( CACHE.localPath( path ) ) )
            ret = log_syscall( "mkdir", mkdir( CACHE.localPath( path ), mode ), 0 );
        // now we remove the "notExist" file, if it exists, so
        // the folder shows up!
        if( ret == 0 )
            CACHE.localFileExist( path );
    }
    log_msg("\n====>mkdir   uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, ret, path);
    return ret;
}

//===================================================================================================================
int hradecFS_unlink(const char *path)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_unlink  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );

    // int ret =  log_syscall("unlink", unlink( CACHE.localPath( path ) ), 0);
    // if( ret == 0)

    CACHE.removeFile( path );

    log_msg("\n====>unlink    uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, 0, path);
    return 0;
}

//===================================================================================================================
int hradecFS_rmdir(const char *path)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_rmdir  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );

    // int ret =  log_syscall("rmdir", rmdir( CACHE.localPath( path ) ), 0);
    // if( ret == 0)

    CACHE.removeDir( path );

    log_msg("\n====>rmdir   uid: [%4d] path: %s \n", fuse_get_context()->uid, path);
    return 0;
}

//===================================================================================================================
int hradecFS_symlink(const char *path, const char *link)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_symlink  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( link );

    int ret =  log_syscall("symlink", symlink(path,  CACHE.localPath( link ) ), 0);
    log_msg("\n====>symlink   uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, ret, path);
    return ret;
}

//===================================================================================================================
int hradecFS_rename(const char *path, const char *newpath,  unsigned int i)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_rename  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );
    CACHE.init( newpath );

    int ret =  log_syscall("rename", rename( CACHE.localPath( path ), CACHE.localPath( newpath ) ), 0);

    log_msg("\n====>rename   uid: [%4d]  return: [%2d] path: %s  newpath: %s \n", fuse_get_context()->uid, ret, path, newpath);
    return ret;
}

//===================================================================================================================
int hradecFS_link(const char *path, const char *newpath)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_link  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );
    CACHE.init( newpath );

    int ret =  log_syscall("link", link( CACHE.localPath( path ), CACHE.localPath( newpath ) ), 0);
    log_msg("\n====>link   uid: [%4d]  return: [%2d] path: %s  newpath: %s \n", fuse_get_context()->uid, ret, path, newpath);
    return ret;
}

//===================================================================================================================
int hradecFS_chmod(const char *path, mode_t mode,  fuse_file_info *fi )
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_chmod  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );

    int ret =  log_syscall("chmod", chmod( CACHE.localPath( path ), mode), 0);

    log_msg("\n====>chmod     uid: [%4d] mode: [0%3o] return: [%2d] path: %s \n",fuse_get_context()->uid,  mode, ret, path);
    return ret;
}

//===================================================================================================================
int hradecFS_chown(const char *path, uid_t uid, gid_t gid,  fuse_file_info *fi)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_chown  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );

    int ret =  log_syscall("chown", chown( CACHE.localPath( path ), uid, gid), 0);

    log_msg("\n====>chown   newUID: [%d] newGID: [%d] uid: [%4d]  return: [%2d] path: %s \n", uid, gid, fuse_get_context()->uid, ret, path);

    return ret;
}

//===================================================================================================================
/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int hradecFS_truncate(const char *path, off_t newsize,  fuse_file_info *fi)
{
     log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_truncate  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
     CACHE.init( path );

     int ret =  log_syscall("truncate", truncate( CACHE.localPath( path ), newsize), 0);
     if( ret == 0 ){
         CACHE.init( path );
         CACHE.localFileExist( path, CACHE.no_uid );
     }
     return ret;
}
int hradecFS_ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_ftruncate  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );

    // log_msg("\nhradecFS_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n", path, size, fi);
    // log_fi(fi);

    int res=-1;

    if (fi != NULL){
        log_msg("\n\tftruncate(path=\"%s\") %d \n", path, size);
        res = ftruncate(fi->fh, size);
    }else{
        log_msg("\n\ttruncate(path=\"%s\") %d \n", path, size);
        res = log_syscall("truncate", truncate( CACHE.localPath( path ), size), 0);
    }
    if( res == 0 ){
        CACHE.setLocallyCreated( path );
        log_msg("\ntruncate(path=\"%s\") - localFileExist\n", path);
        CACHE.localFileExist( path, CACHE.no_uid );
    }else{
        res=-errno;
    }
    log_msg("\n====>ftruncate   size: [%lld] uid: [%4d] return: [%2d] path: %s \n", size, fuse_get_context()->uid, res, path);

    return res;
}


//===================================================================================================================
int hradecFS_utime(const char *path, struct utimbuf *ubuf)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_utime  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );

    log_msg("\nhradecFS_utime(path=\"%s\", ubuf=0x%08x)\n", CACHE.localPath( path ), ubuf);

    int ret =  log_syscall("utime", utime( CACHE.localPath( path ), ubuf ), 0);
    return ret;
}

#ifdef HAVE_UTIMENSAT
static int hradecFS_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi)
{
    (void) fi;
    int res;

    // don't use utime/utimes since they follow symlinks
    res = utimensat(0, CACHE.localPath( path ), ts, AT_SYMLINK_NOFOLLOW);
    if (res == -1)
        res = -errno;

    log_msg("\n====>utime     uid: [%4d]  return: [%2d] path: %s \n",fuse_get_context()->uid, res, path);

    return res;
}
#endif



//===================================================================================================================
/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int hradecFS_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    int fd=0;
    int count=0;
    char tmp[PATH_MAX*3];
    char cmd[PATH_MAX*3];

    long remoteSize = 0;
    char rsyncIt=0;

    CACHE.init( path );
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_open  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    log_msg( "\nhradecFS_open(CACHE.localPath(\"%s\") - %s\n", CACHE.localPath(path), path );

    // check if a log file has "100%", which means the file is synced OK!!
    // if this is the case, just skip all the shananigans!
    if( CACHE.fileInSync(path) ){
        log_msg( "\n !!! already cached %s = %s\n", path, CACHE.localPathLog(path));

    // we don't have the file locally, so lets cache it!
    // this is the ONLY place were we read file content from files in the remote side!!!
    }else{
        // double check if the file is not in sync now... if not, keep trying to retrieve!!
        while ( ! CACHE.fileInSync(path) ) {
            remoteSize = getFileSize(CACHE.remotePath(path));
            log_msg("\n\n\n=====================================================================\n" );

            // if remote file doesn't exist, don't try to transfer it!!
            // this should account for when a file disappears from the remote filesystem during a transfer!
            if( ! CACHE.existsRemote( path ) ){
                log_msg( "\n %s doesn't exist remotely, so can't rsync!\n", CACHE.remotePath(path) );
                break;
            }

            rsyncIt=0;
            log_msg("\nrsyncIt: %d\n", rsyncIt );
            while( 1 ) {
                // create a look file for the current file only,
                // so we can run rsync without lockint all threads, just the ones trying
                // to open the same file
                log_msg("\nexists(cacheFileLock): %s=%d\n", CACHE.localPathLock(path), exists(CACHE.localPathLock(path)) );

                // lock so only one thread can run rsync at a time!
                pthread_mutex_lock(&mutex);
                if( ! exists(CACHE.localPathLock(path)) ){
                    rsyncIt=0;
                    // double check if file is not already in sync!
                    // if ( getFileSize(CACHE.localPath(path)) != remoteSize ) {
                    if( ! CACHE.fileInSync(path) ){
                        // create a lock file inside the mutex, so we can use
                        // to make others proccesses accessing this file wait rsync to finish!
                        close( creat( CACHE.localPathLock(path), S_IRWXU | S_IRWXG | S_IRWXO ) );
                        // and now set it to rsync, so we can remove the lock!
                        rsyncIt=1;

                        // if localpath doesn't exist, create it!
                        if( ! isdir( CACHE.localPathDir(path) ) ){
                            mkdir( CACHE.localPathDir(path), S_IRWXU | S_IRWXG | S_IRWXO );
                        }
                    }
                    // unlock threads and break out from the loop!
                    // this it the ONLY out of this loop! (when lock file doesn't exist anymore!)
                    // if at this point rsyncIt is 0, we just open the file
                    // or else, we run rsync!
                    pthread_mutex_unlock(&mutex);
                    break;
                }
                // if theres a fileLock, just unlock threads and wait!
                pthread_mutex_unlock(&mutex);
                // waiting without locks allow for other threads to open files
                // and run other rsyncs!
                sleep(2);
            }

            // if rsyncIt=1, we have a lockFile and we need to run rsync
            // all other threads trying to access this sane file are waiting the
            // lock file to be removed!!
            if ( rsyncIt ) {
                log_msg("\n\n!!! STARTED RSYNC\n\n");
                // reset the log file!
                CACHE.localFileExist( path, CACHE.no_uid );
                // construct the rsync command call!
                string from = string("'") + CACHE.remotePath(path) + "'";
                string to   = string("'") + CACHE.localPath(path) + "'";
                sprintf( tmp, BB_DATA->syncCommand, from.c_str(), to.c_str() );
                // and pipe the output to the log file!
                sprintf( cmd, "%s >> '%s' 2>&1", tmp, CACHE.localPathLog(path) );

                // run the cmd now!
                log_msg( cmd );
                log_msg("\n\t!!! rsync system return: [%2d]", system( cmd ) );

                // we need to make sure the cache from rsync is flushed before
                // we can open it here
                // TODO: we need to implement the same, but with SYNCFS() instead, so
                //       not all filesystems need to be flushed. Just the one!!
                sync();

                // rsync done, so we lock threads and remove lockfile!
                pthread_mutex_lock(&mutex);
                remove( CACHE.localPathLock(path) );
                // sprintf( cmd, "rm  %s\n", CACHE.localPathLock(path) );
                // system( cmd );
                pthread_mutex_unlock(&mutex);

                log_msg("\n\n!!! FINISHED RSYNC\n\n");
            }

        }
    }

    // now we can FINALLY open the file!!
    fd = open(CACHE.localPath(path), fi->flags);
    if (fd < 0)
        retstat = log_error("open");

    fi->fh = fd;

    // log_fi(fi);
    log_msg("\n====>open      uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, retstat, path);


    return retstat;
}

//===================================================================================================================
int hradecFS_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    // int retstat = 0;
    // log_msg("\nhradecFS_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
    //     path, buf, size, offset, fi);
    // // no need to get fpath on this one, since I work from fi->fh not the path
    // log_fi(fi);
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_read  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);

    int ret=0;

    if( string( path ) == "/.hradecFS_local_files" ){
        string p;
        string files = "";
        sprintf( buf, "" );
        vector<string> zfiles = glob(CACHE._cacheControl() + "*.__local__" );
        for ( unsigned int i = 0; i < zfiles.size(); i++ ) {
            if( int( zfiles[i].find(".hradecFS_") ) < 0 ){
                p = CACHE.getPathFromLogFile( zfiles[i] );
                if( isfile( p.c_str() ) )
                    sprintf( buf, string( p + "\n" + buf ).c_str() );
            }
        }
        ret = log_syscall("====>pread", strlen(buf), 0);
    }else{
        ret = pread(fi->fh, buf, size, offset);
        log_msg("\n====>pread     uid: [%4d] size: [%10lld] offset: [%10lld] read: [%10lld]   size=read: %d  path: %s \n", fuse_get_context()->uid, size, offset, ret, size==ret,   path);
    }

    return ret;
}

//===================================================================================================================
int hradecFS_write(const char *path, const char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    // int retstat = 0;
    // log_msg("\nhradecFS_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
    //     path, buf, size, offset, fi
    //     );
    // // no need to get fpath on this one, since I work from fi->fh not the path
    // log_fi(fi);
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_write  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.setLocallyCreated( path );

    int ret = log_syscall("pwrite", pwrite(fi->fh, buf, size, offset), 0);
    log_msg("\n====>pwrite    uid: [%4d]  size: [%10lld] offset: [%10lld] write: [%10lld] size=read: %d  path: %s \n", fuse_get_context()->uid, size, offset, ret, size==ret, path);

    // we need to update the log file here to reflect the new size of the file right
    // after writing, since NFS call getattr right after writing, before closing the file.
    CACHE.localFileExist( path );
    return ret;
}

//===================================================================================================================
int hradecFS_statfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_statfs  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );

    // char fpath[PATH_MAX];
    // log_msg("\nhradecFS_statfs(path=\"%s\", statv=0x%08x)\n",
    //     path, statv);
    // hradecFS_fullpath(fpath, path);

    // get stats for underlying filesystem
    retstat = log_syscall("statvfs",
        statvfs( CACHE.localPath( path ), statv)
    , 0);

    log_statvfs(statv);

    log_msg("\n====>statfs    uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, retstat, path);

    return retstat;
}

//===================================================================================================================
// /* This is called from every close on an open file, so call the
//     close on the underlying filesystem.  But since flush may be
//     called multiple times for an open file, this must not really
//     close the file.  This is important if used on a network
//     filesystem like NFS which flush the data/metadata on close() */
int hradecFS_flush(const char *path, struct fuse_file_info *fi)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_flush  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    int res = -1;

    if( fi->fh != NULL )
        res = log_syscall("fsync", fsync(fi->fh), 0);

    if( res == 0 )
        CACHE.localFileExist( path );

    log_msg("\n====>flush     uid: [%4d]  path: %s\n", fuse_get_context()->uid, path);
    return 0;
}

//===================================================================================================================
int hradecFS_release(const char *path, struct fuse_file_info *fi)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_release  %s - [%f]\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    // log_msg("\nhradecFS_release(path=\"%s\", fi=0x%08x)\n",
    //   path, fi);
    // log_fi(fi);

    // We need to close the file.  Had we allocated any resources
    // (buffers etc) we'd need to free them here as well.
    int ret = log_syscall("close", close(fi->fh), 0);
    if( ret == 0 ){
        CACHE.init( path );
        CACHE.localFileExist( path, CACHE.no_uid );
    }
    log_msg("\n====>release   uid: [%4d]  return: [%2d] path: %s \n", fuse_get_context()->uid, ret, path);

    return ret;
}

//===================================================================================================================
int hradecFS_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_fsync  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    // log_msg("\nhradecFS_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
    //     path, datasync, fi);
    // log_fi(fi);

    // some unix-like systems (notably freebsd) don't have a datasync call
    int res = -1;
    #ifdef HAVE_FDATASYNC
        if (datasync)
            res = log_syscall("fdatasync", fdatasync(fi->fh), 0);
        else
    #endif
        res = log_syscall("fsync", fsync(fi->fh), 0);

    if( res == 0 )
        CACHE.localFileExist( path );

    log_msg("\n====>fsync     uid: [%4d]  return: [%2d] path: %s\n", fuse_get_context()->uid, res, path);
    return res;
}

//===================================================================================================================
#ifdef HAVE_SYS_XATTR_H
    /** Set extended attributes */
    int hradecFS_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
    {
        log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_setxattr  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
        char fpath[PATH_MAX];

        log_msg("\nhradecFS_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n",
            path, name, value, size, flags);
        // hradecFS_fullpath(fpath, path);
        CACHE.init( path );

        int ret = log_syscall("lsetxattr", lsetxattr(CACHE.localPath( path ), name, value, size, flags), 0);

        log_msg("\n====>setxtr uid: [%4d]  return: [%2d] path: %s\n", fuse_get_context()->uid, ret, path);
        return ret;
    }

    //===================================================================================================================
    /** Get extended attributes */
    int hradecFS_getxattr(const char *path, const char *name, char *value, size_t size)
    {
        int retstat = 0;
        char fpath[PATH_MAX];
        log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_getxattr  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);

        log_msg("\nhradecFS_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
            path, name, value, size);
        // hradecFS_fullpath(fpath, path);
        CACHE.init( path );

        retstat = log_syscall("lgetxattr", lgetxattr(CACHE.localPath( path ), name, value, size), 0);
        if (retstat >= 0)
        log_msg("    value = \"%s\"\n", value);

        log_msg("\n====>getxtr uid: [%4d]  return: [%2d] path: %s\n", fuse_get_context()->uid, retstat, path);
        return retstat;
    }

    //===================================================================================================================
    /** List extended attributes */
    int hradecFS_listxattr(const char *path, char *list, size_t size)
    {
        int retstat = 0;
        char fpath[PATH_MAX];
        char *ptr;
        log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_listxattr  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);

        log_msg("hradecFS_listxattr(path=\"%s\", list=0x%08x, size=%d)\n",
            path, list, size
            );
        // hradecFS_fullpath(fpath, path);
        CACHE.init( path );

        retstat = log_syscall("llistxattr", llistxattr(CACHE.localPath( path ), list, size), 0);
        if (retstat >= 0) {
        log_msg("    returned attributes (length %d):\n", retstat);
        for (ptr = list; ptr < list + retstat; ptr += strlen(ptr)+1)
            log_msg("    \"%s\"\n", ptr);
        }

        log_msg("\n====>lstxtr uid: [%4d]  return: [%2d] path: %s\n", fuse_get_context()->uid, retstat, path);
        return retstat;
    }

    //===================================================================================================================
    /** Remove extended attributes */
    int hradecFS_removexattr(const char *path, const char *name)
    {
        char fpath[PATH_MAX];
        log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_removexattr  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);

        log_msg("\nhradecFS_removexattr(path=\"%s\", name=\"%s\")\n",
            path, name);
        // hradecFS_fullpath(fpath, path);
        CACHE.init( path );

        int retstat = log_syscall("lremovexattr", lremovexattr(CACHE.localPath( path ), name), 0);
        log_msg("\n====>remxtr uid: [%4d]  return: [%2d] path: %s\n", fuse_get_context()->uid, retstat, path);
        return retstat;
    }
#endif



//===================================================================================================================
int hradecFS_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_fsyncdir  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);

    // log_msg("\nhradecFS_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
        // path, datasync, fi);
    // log_fi(fi);

    log_msg("\n====>fsyncdir  uid: [%4d]  return: [%2d] path: %s\n", fuse_get_context()->uid, retstat, path);
    return retstat;
}

//===================================================================================================================
void hradecFS_destroy(void *userdata)
{
    log_msg( "\nhradecFS_destroy(userdata=0x%08x)\n", userdata );
    log_msg("\n====>destroy   uid: [%4d]\n", fuse_get_context()->uid);
}

//===================================================================================================================
/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int hradecFS_access(const char *path, int mask)
{
    int retstat = -1;
    const char *__path=path;
    char fpath[PATH_MAX];
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_access  %s [%d] \n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path, fuse_get_context()->uid);
    CACHE.init( path );

    if( CACHE.existsRemote( path ) ){

        retstat = access( CACHE.localPath( path ), mask );
    }

    log_msg("\nhradecFS_access(path=\"%s\", mask=0%o, retstat=%d)\n",
        CACHE.localPath( path ), mask, retstat);

    if (retstat < 0)
       retstat = log_error("hradecFS_access access");

    log_msg("\n====>access    uid: [%4d]  return: [%2d] path: %s\n", fuse_get_context()->uid, retstat, path);
    return retstat;
}


//===================================================================================================================
void *hradecFS_init(struct fuse_conn_info *conn, fuse_config *fc)
{
    log_conn( conn );
    log_fuse_context( fuse_get_context() );

    pthread_mutex_lock(&mutex);
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_init  \n>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    CACHE.cleanupBeforeStart();

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_init  \n>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    CACHE.cleanupCache();
    pthread_mutex_unlock(&mutex);

    // do the initial cache of the root folder
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_init  \n>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    CACHE.doCachePathDir( "/" );

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_init Finished \n>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    // std::thread t1(task1, cleanupCache);

    return BB_DATA;
}

//===================================================================================================================
struct hradecFS_oper_struc : fuse_operations  {
    hradecFS_oper_struc() {
        getattr  = hradecFS_getattr;
        readlink = hradecFS_readlink;
        // no .getdir -- that's deprecated
        // getdir = NULL;
        mknod    = hradecFS_mknod;
        mkdir    = hradecFS_mkdir;
        unlink   = hradecFS_unlink;
        rmdir    = hradecFS_rmdir;
        symlink  = hradecFS_symlink;
        rename   = hradecFS_rename;
        link     = hradecFS_link;
        chmod    = hradecFS_chmod;
        chown    = hradecFS_chown;
        truncate = hradecFS_truncate;
        open     = hradecFS_open;
        read     = hradecFS_read;
        write    = hradecFS_write;
        /** Just a placeholder; don't set */ // huh???
        statfs   = hradecFS_statfs;
        flush    = hradecFS_flush;
        release  = hradecFS_release;
        fsync    = hradecFS_fsync;
        #ifdef HAVE_UTIMENSAT
            utimens     = hradecFS_utimens;
        #endif
        #ifdef HAVE_SYS_XATTR_H
            setxattr    = hradecFS_setxattr;
            getxattr    = hradecFS_getxattr;
            listxattr   = hradecFS_listxattr;
            removexattr = hradecFS_removexattr;
        #endif

        opendir    = hradecFS_opendir;
        readdir    = hradecFS_readdir;
        // releasedir = hradecFS_releasedir;
        // fsyncdir   = hradecFS_fsyncdir;
        init       = hradecFS_init;
        destroy    = hradecFS_destroy;
        access     = hradecFS_access;
        truncate   = hradecFS_ftruncate;
        // fgetattr   = hradecFS_fgetattr;
    }
};
static struct hradecFS_oper_struc hradecFS_oper;


//
// struct fuse_operations hradecFS_oper = {
//   .getattr = hradecFS_getattr,
//   .readlink = hradecFS_readlink,
//   // no .getdir -- that's deprecated
//   //.getdir = NULL,
//   .mknod = hradecFS_mknod,
//   .mkdir = hradecFS_mkdir,
//   .unlink = hradecFS_unlink,
//   .rmdir = hradecFS_rmdir,
//   .symlink = hradecFS_symlink,
//   .rename = hradecFS_rename,
//   .link = hradecFS_link,
//   .chmod = hradecFS_chmod,
//   .chown = hradecFS_chown,
//   .truncate = hradecFS_truncate,
//   // .utime = hradecFS_utime,
//   .open = hradecFS_open,
//   .read = hradecFS_read,
//   .write = hradecFS_write,
//   /** Just a placeholder, don't set */ // huh???
//   .statfs = hradecFS_statfs,
//   .flush = hradecFS_flush,
//   .release = hradecFS_release,
//   .fsync = hradecFS_fsync,
//
// #ifdef HAVE_SYS_XATTR_H
//   .setxattr = hradecFS_setxattr,
//   .getxattr = hradecFS_getxattr,
//   .listxattr = hradecFS_listxattr,
//   .removexattr = hradecFS_removexattr,
// #endif
//
//   .opendir = hradecFS_opendir,
//   .readdir = hradecFS_readdir,
//   .releasedir = hradecFS_releasedir,
//   .fsyncdir = hradecFS_fsyncdir,
//   .init = hradecFS_init,
//   .destroy = hradecFS_destroy,
//   .access = hradecFS_access,
//   .ftruncate = hradecFS_ftruncate,
//   .fgetattr = hradecFS_fgetattr
// };

void hradecFS_usage()
{
    fprintf(stderr, "usage:  bbfs [FUSE and mount options] rootDir mountPoint\n");
    exit(-1);
}



static char __cacheDir[PATH_MAX];
static char __syncCommand[PATH_MAX];
static char __syncCommand_tmp[PATH_MAX];
int main(int argc, char *argv[])
{
    int fuse_stat, nn=0;
    struct hradecFS_state *hradecFS_data;
    char buff[8192];
    char mountdir[8192];
    char cachedir[8192];
    char rootdir[8192];


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


    // bbfs doesn't do any access checking on its own (the comment
    // blocks in fuse.h mention some of the functions that need
    // accesses checked -- but note there are other functions, like
    // chown(), that also need checking!).  Since running bbfs as root
    // will therefore open Metrodome-sized holes in the system
    // security, we'll check if root is trying to mount the filesystem
    // and refuse if it is.  The somewhat smaller hole of an ordinary
    // user doing it with the allow_other flag is still there because
    // I don't want to parse the options string.

    // if ((getuid() == 0) || (geteuid() == 0)) {
    //  fprintf(stderr, "Running BBFS as root opens unnacceptable security holes\n");
    //  return 1;
    // }

    // See which version of fuse we're running
    fprintf(stderr, "\n%sFuse library version %d.%d\n", bgreen.c_str(), FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);

    // Perform some sanity checking on the command line:  make sure
    // there are enough arguments, and that neither of the last two
    // start with a hyphen (this will break if you actually have a
    // rootpoint or mountpoint whose name starts with a hyphen, but so
    // will a zillion other programs)
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
        hradecFS_usage();


    hradecFS_data = (struct hradecFS_state *) malloc(sizeof(struct hradecFS_state));
    if (hradecFS_data == NULL) {
        perror("main calloc");
        exit(-1);
    }

    // Pull the rootdir out of the argument list and save it in my
    // internal data
    realpath(argv[argc-2], rootdir);
    realpath(argv[argc-1], mountdir);
    //sprintf(rootdir, "%s", argv[argc-2]);
    //sprintf(mountdir, "%s", argv[argc-1]);
    hradecFS_data->rootdir  = rootdir;
    hradecFS_data->mountdir = mountdir;
    hradecFS_data->cachedir = __cacheDir; //
    hradecFS_data->syncCommand = __syncCommand;
    hradecFS_data->log = true;


    sprintf(__cacheDir,"%s_cachedir", hradecFS_data->mountdir);
    //sprintf(__syncCommand,"/bin/rsync -avpPz -e '/bin/ssh -p 22002' atomo2.no-ip.org:/ZRAID/atomo/%%s %%s");
    sprintf(__syncCommand,"/bin/rsync -avpP %%s %%s");
    // fprintf(stderr, "1\n");

    char *nargv[10];
    int nargc=0;
    for(nn=0;nn<argc;nn++){
        if(argv[nn][0] == '-'){
            if(strcmp(argv[nn],"--command")==0){
                sprintf(__syncCommand,  argv[nn+1]);
                nn++;
            }else if(strcmp(argv[nn],"--cache")==0){
                sprintf(__cacheDir, argv[nn+1]);
                nn++;
            }else{
                nargv[nargc] = (char *) calloc(strlen(argv[nn])+1, sizeof(char));
                strcpy(nargv[nargc++], argv[nn]);
            }
        }else{
            nargv[nargc] = (char *) calloc(strlen(argv[nn])+1, sizeof(char));
            strcpy(nargv[nargc++], argv[nn]);
        }
        // fprintf(stderr, "..%d...%s..\n", strlen(nargv[nn])+1, nargv[nn]);
    }

    // create a log pipe at /tmp/.bbfs.log
    // so if we want to see the log, just run "tail -f /tmp/.bbfs.log"
    hradecFS_data->logfile = log_open_pipe();

    fprintf( stderr, "\n");
    fprintf( stderr, "%shradecFS_data->rootdir     : [ %s ]\n", byellow.c_str(), (bblue+hradecFS_data->rootdir+byellow).c_str() );
    fprintf( stderr, "%shradecFS_data->syncCommand : [ %s ]\n", byellow.c_str(), (bblue+hradecFS_data->syncCommand+byellow).c_str() );
    fprintf( stderr, "%shradecFS_data->mountdir    : [ %s ]\n", byellow.c_str(), (bblue+hradecFS_data->mountdir+byellow).c_str() );
    fprintf( stderr, "%shradecFS_data->cachedir    : [ %s ]\n", byellow.c_str(), (bblue+hradecFS_data->cachedir+byellow).c_str() );
    fprintf( stderr, (reset+"\n").c_str() );


    mkdir_p( hradecFS_data->mountdir, 0777 ) ;
    mkdir_p( hradecFS_data->cachedir, 0777 ) ;



    // turn over control to fuse
    // fprintf( stderr, "about to call fuse_main\n\n%s", reset.c_str() );
    strcpy( nargv[nargc-2], nargv[nargc-1] );
    nargv[nargc-1][0] = 0;
    nargc--;

    fuse_stat = fuse_main(nargc, nargv, &hradecFS_oper, hradecFS_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    return fuse_stat;
}

} // extern "C"
