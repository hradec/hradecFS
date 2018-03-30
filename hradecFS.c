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




void task1(void (*func)() ) {
    log_msg("\nmultithread --> task1\n");
    (*func)();
    sleep(60);
}


void cleanupCache(){
    CACHE.cleanupCache();
}



///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come from /usr/include/fuse.h
//
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */

int hradecFS_getattr(const char *path, struct stat *statbuf, fuse_file_info *file_info)
{
    (void) file_info;
    int retstat=-1;
    char fpath[PATH_MAX];


    if( string(path) == "/.logoff" ) {
        log_msg("log off");
        BB_DATA->log = false;
    }else if( string(path) == "/.logon" ) {
        BB_DATA->log = true;
        log_msg("log on");
    }

    CACHE.init( path );

    // if path exists (the exist cache in dev/shm/bbfs._)
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_getattr %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    if ( CACHE.existsRemote( path ) ){

        // if we have the file cached (just a placeholder or the actual full file, doesn't matter)
        if( ! CACHE.existsLocal( path ) ){
            // CACHE.doCachePathParentDir( path );
            // CACHE.doCachePath( path, statbuf );
            log_msg("\nREMOTE  hradecFS_getattr_cache( CACHE.doCachePathParentDir(%s) )\n", CACHE.localPath( path ));
        }
    }
    retstat = CACHE.stat( path, statbuf );

    log_msg("\nREMOTE RETURN  hradecFS_getattr_cache - %s - return %d", path, retstat);
    return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int hradecFS_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int retstat = 0;
    char fpath[PATH_MAX];

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_opendir %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);

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
        log_msg("CACHE.doCachePathDir(%s)   hradecFS_opendir(path=\"%s\", fi=0x%08x)\n",path, CACHE.remotePath(path), fi);
    }


    if( CACHE.existsLocal( path ) ){
        log_msg("\nopendir( CACHE.localPath(%s)=%s )\n",path, CACHE.localPath(path));
        dp = opendir(CACHE.localPath(path));
    }

    // log_msg("    opendir returned 0x%p\n", dp);
    if (dp == NULL)
       retstat = log_error("hradecFS_opendir opendir");

    fi->fh = (intptr_t) dp;

    log_fi(fi);
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

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

    CACHE.init( path );
    CACHE.doCachePathDir( path );
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_readdir  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);

    // log_msg("\nhradecFS_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",path, buf, filler, offset, fi);
    // once again, no need for fullpath -- but note that I need to cast fi->fh
    // dp = (DIR *) (uintptr_t) fi->fh;

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
    while ((de = readdir(dp)) != NULL) {
        string entry = string(path)+"/"+de->d_name;
        CACHE.init(entry);

        log_msg("\033[1;31m calling filler with name %s\n", de->d_name);
        if( string(de->d_name)==".." || string(de->d_name)=="." || CACHE.existsRemote( entry ) ){
            struct stat st;
            memset(&st, 0, sizeof(st));
            st.st_ino = de->d_ino;
            st.st_mode = de->d_type << 12;
            // lstat( CACHE.localPath(entry), &st );
            if ( filler( buf, de->d_name, &st, 0, 0 ) ) {
                log_msg( "\033[1;31m      >>>>> ERROR hradecFS_readdir filler:  buffer full" );
                // return -ENOMEM;
                break;
            }
        }
    } //while ((de = readdir(dp)) != NULL);

    closedir( dp );
    log_fi(fi);

    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int hradecFS_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_releasedir  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    // CACHE.init( path );
    log_fi(fi);
    closedir((DIR *) (uintptr_t) fi->fh);
    return retstat;
}


/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to hradecFS_readlink()
// hradecFS_readlink() code by Bernardo F Costa (thanks!)
int hradecFS_readlink(const char *path, char *link, size_t size)
{
    int retstat;
    char fpath[PATH_MAX];

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_readlink  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
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
    return retstat;
}


/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
// shouldn't that comment be "if" there is no.... ?
int hradecFS_mknod(const char *path, mode_t mode, dev_t dev)
{
    int retstat=-1;

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\nhradecFS_mknod  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    CACHE.init( path );


    log_msg("\nhradecFS_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
        CACHE.localPath( path ), mode, dev);


    // On Linux this could just be 'mknod(path, mode, dev)' but this
    // tries to be be more portable by honoring the quote in the Linux
    // mknod man page stating the only portable use of mknod() is to
    // make a fifo, but saying it should never actually be used for
    // that.
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
        int file = creat( CACHE.localPathLog( path ), mode );
        string tmp = _format( "%s*%lld\n100%\n)", CACHE.localPath( path ) , -1 );
        write( file, tmp.c_str(),  tmp.length() );
        close( file );
        CACHE.localFileNotExistRemove( path );

    }


    return retstat;
}

/** Create a directory */
int hradecFS_mkdir(const char *path, mode_t mode)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_mkdir  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);

    CACHE.init( path );

    // char fpath[PATH_MAX];
    log_msg("\nhradecFS_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);
    // hradecFS_fullpath(fpath, path);

    return log_syscall("mkdir", mkdir( CACHE.localPath( path ), mode), 0);
}

/** Remove a file */
int hradecFS_unlink(const char *path)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_unlink  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    CACHE.init( path );

    // char fpath[PATH_MAX];
    // log_msg("hradecFS_unlink(path=\"%s\")\n",
    //     path);
    // hradecFS_fullpath(fpath, path);

    int ret =  log_syscall("unlink", unlink( CACHE.localPath( path ) ), 0);
    if( ret == 0){
        CACHE.removeFile( path );
    }
    return ret;
}

/** Remove a directory */
int hradecFS_rmdir(const char *path)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_rmdir  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    CACHE.init( path );

    // char fpath[PATH_MAX];
    // log_msg("hradecFS_rmdir(path=\"%s\")\n",
    //     path);
    // hradecFS_fullpath(fpath, path);

    int ret =  log_syscall("rmdir", rmdir( CACHE.localPath( path ) ), 0);
    if( ret == 0){
        CACHE.removeDir( path );
    }
    return ret;
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int hradecFS_symlink(const char *path, const char *link)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_symlink  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    CACHE.init( link );

    // char flink[PATH_MAX];
    // log_msg("\nhradecFS_symlink(path=\"%s\", link=\"%s\")\n",
    //     path, link);
    // hradecFS_fullpath(flink, link);

    int ret =  log_syscall("symlink", symlink(path,  CACHE.localPath( link ) ), 0);
    return ret;
}

/** Rename a file */
// both path and newpath are fs-relative
int hradecFS_rename(const char *path, const char *newpath,  unsigned int i)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_rename  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    CACHE.init( path );
    CACHE.init( newpath );

    // char fpath[PATH_MAX];
    // char fnewpath[PATH_MAX];
    // log_msg("\nhradecFS_rename(fpath=\"%s\", newpath=\"%s\")\n",
    //     path, newpath);
    // hradecFS_fullpath(fpath, path);
    // hradecFS_fullpath(fnewpath, newpath);

    int ret =  log_syscall("rename", rename( CACHE.localPath( path ), CACHE.localPath( newpath ) ), 0);
    return ret;
}

/** Create a hard link to a file */
int hradecFS_link(const char *path, const char *newpath)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_link  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    CACHE.init( path );
    CACHE.init( newpath );

    // char fpath[PATH_MAX], fnewpath[PATH_MAX];
    // log_msg("\nhradecFS_link(path=\"%s\", newpath=\"%s\")\n",
    //     path, newpath);
    // hradecFS_fullpath(fpath, path);
    // hradecFS_fullpath(fnewpath, newpath);

    int ret =  log_syscall("link", link( CACHE.localPath( path ), CACHE.localPath( newpath ) ), 0);
    return ret;
}

/** Change the permission bits of a file */
int hradecFS_chmod(const char *path, mode_t mode,  fuse_file_info *fi )
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_chmod  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    CACHE.init( path );

    // char fpath[PATH_MAX];
    // log_msg("\nhradecFS_chmod(fpath=\"%s\", mode=0%03o)\n",
    //     path, mode);
    // hradecFS_fullpath(fpath, path);

    int ret =  log_syscall("chmod", chmod( CACHE.localPath( path ), mode), 0);
    return ret;
}

/** Change the owner and group of a file */
int hradecFS_chown(const char *path, uid_t uid, gid_t gid,  fuse_file_info *fi)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_chown  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    CACHE.init( path );

    // char fpath[PATH_MAX];
    // log_msg("\nhradecFS_chown(path=\"%s\", uid=%d, gid=%d)\n",
    //     path, uid, gid);
    // hradecFS_fullpath(fpath, path);

    int ret =  log_syscall("chown", chown( CACHE.localPath( path ), uid, gid), 0);
    return ret;
}

/** Change the size of a file */
int hradecFS_truncate(const char *path, off_t newsize,  fuse_file_info *fi)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_truncate  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    CACHE.init( path );

    // char fpath[PATH_MAX];
    // log_msg("\nhradecFS_truncate(path=\"%s\", newsize=%lld)\n",
    //     path, newsize);
    // hradecFS_fullpath(fpath, path);

    int ret =  log_syscall("truncate", truncate( CACHE.localPath( path ), newsize), 0);
    return ret;
}

/** Change the access and/or modification times of a file */
/* note -- I'll want to change this as soon as 2.6 is in debian testing */
int hradecFS_utime(const char *path, struct utimbuf *ubuf)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_utime  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    CACHE.init( path );

    // char fpath[PATH_MAX];
    // log_msg("\nhradecFS_utime(path=\"%s\", ubuf=0x%08x)\n",
    //     path, ubuf);
    // hradecFS_fullpath(fpath, path);

    int ret =  log_syscall("utime", utime( CACHE.localPath( path ), ubuf ), 0);
    return ret;
}

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
    char fpath[PATH_MAX];
    char tmp[PATH_MAX*3];
    char cmd[PATH_MAX*3];
    char cacheFile[PATH_MAX*2];
    char cacheFileLock[PATH_MAX*2];
    char cacheDir[PATH_MAX*2];
    char buff[PATH_MAX*2];
    char pathBasename[PATH_MAX*2];
    char pathDir[PATH_MAX*2];

    char custom_cmd[PATH_MAX*2];

    long remoteSize = 0;

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_open  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);

    CACHE.init( path );

    // log_msg("\nhradecFS_open(path\"%s\", fi=0x%08x)\n", path, fi);
    // hradecFS_fullpath(fpath, path);

    /* TODO:
            checar o upload folder se tem o arquivo q queremos cachear... se tiver, pega de la ao inves de pegar do server,
            pois o folder de upload e mais novo q o server!
    */

    //sprintf(cacheDir, BB_DATA->cachedir);
    //sprintf(cacheFile,"%s%s", cacheDir, path);
    sprintf(cacheFile,"%s", CACHE.localPath(path));
    char *_pathDir       = dirname(strcpy(pathDir,cacheFile));
    char *_pathBasename  = basename(strcpy(pathBasename,cacheFile));
    sprintf( cacheFileLock, "%s_locked", CACHE.localPath(path) );


    log_msg( "\nhradecFS_open(CACHE.localPath(\"%s\") - %s\n", CACHE.localPath(path), path );

    // copy file to local cache, if its not there or size doesn't match!!!
    count=0;

    // check if a .bbfslog file has 100%, which means rsync transfered the file 100% suscessfully!
    // sprintf( cmd, "%s.bbfslog", CACHE.localPath(path) );
    if( exists( CACHE.localPathLog(path) ) && grep( CACHE.localPathLog(path), "100%" ) && CACHE.fileInSync(path) ){
        log_msg( "\nalready cached = %s\n", CACHE.localPathLog(path));

    // we don't have the file locally, so lets cache it!
    }else{
        /// URGENT:
        /// this does not account for files that exist locally, but NOT remotely
        // Those files work by accident, but they for this checkFileSize fucntion to
        // allways look remotely, even if we already now they don't exist there!!
        // we need a better way to determine what is a in-sync file, and what's not, instead of the local->remote file size comparing.

        // checkFileSize needs to become CACHE.fileInSync()
        while ( ! CACHE.fileInSync(path) ) {
            remoteSize = getFileSize(CACHE.remotePath(path));
            log_msg("\n\n\n=====================================================================\n" );

            // if remote file doesn't exist, don't try to transfer it!!
            // this should account for when a file disappears from the remote filesystem during a transfer!
            // if( ! exists(CACHE.remotePath(path)) ){
            if( ! CACHE.existsRemote( path ) ){
                log_msg( "\n %s doesn't exist remotely, so can't rsync!\n", CACHE.remotePath(path) );
                break;
            }

            char rsyncIt=0;
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
                    if ( getFileSize(CACHE.localPath(path)) != remoteSize ) {
                        // if theres a fileLock, just unlock threads and wait!
                        sprintf( cmd, "touch  %s\n", CACHE.localPathLock(path) );
                        system( cmd );
                        if( ! isdir(_pathDir) ){
                            sprintf( cmd, "/bin/mkdir -p %s", pathDir );
                            system( cmd );
                            sprintf( cmd, "/bin/chmod a+rwx %s", pathDir );
                            system( cmd );
                        }
                        rsyncIt=1;
                    }
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
                // struct stat fpath_st;
                // stat(fpath, &fpath_st);
                // if( fpath_st.st_size < 10000 ){
                //     sprintf( cmd, "rsync -avpP %s %s  > %s.bbfslog 2>&1 \n\n", fpath, cacheFile, cacheFile );
                //     log_msg( cmd );
                //     log_msg("cp return: %d", system( cmd ) );
                //
                // }else
                {
                    //sprintf( cmd, "\n\nCustom Command: %s  > %s.bbfslog 2>&1 \n\n", custom_cmd, cacheFile );
                    sprintf( tmp, BB_DATA->syncCommand, CACHE.remotePath(path), CACHE.localPath(path) );
                    sprintf( cmd, "%s >> %s", tmp, CACHE.localPathLog(path) );
                    log_msg( cmd );
                    log_msg("\nrsync system return: %d", system( cmd ) );
                }

                sleep(1);
                count=0;
                while( remoteSize != getFileSize(CACHE.localPath(path)) && count<5  ){
                    sleep(1);
                    count++;
                }

                if( count>4 ) {
                    log_msg("\n\ntransfer failed!!!\n\n");

                }

                // rsync done, so we lock threads and remove lockfile!
                pthread_mutex_lock(&mutex);
                sprintf( cmd, "rm  %s\n", CACHE.localPathLock(path) );
                system( cmd );
                pthread_mutex_unlock(&mutex);

                log_msg("\n\nFINISHED RSYNC\n\n");
            }

        }
    }

    // log_msg( "\nBB_DATA->cachedir=%s", BB_DATA->cachedir);
    //log_msg( "\ncacheFile=%s - %d", cacheFile, fi->flags);

    // fd = log_syscall("===>open", open(cacheFile, fi->flags), 0);
    fd = open(CACHE.localPath(path), fi->flags);
    if (fd < 0)
        retstat = log_error("open");

    fi->fh = fd;

    log_fi(fi);


    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int hradecFS_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    // int retstat = 0;
    // log_msg("\nhradecFS_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
    //     path, buf, size, offset, fi);
    // // no need to get fpath on this one, since I work from fi->fh not the path
    // log_fi(fi);
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_read  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);

    return log_syscall("====>pread", pread(fi->fh, buf, size, offset), 0);
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
// As  with read(), the documentation above is inconsistent with the
// documentation for the write() system call.
int hradecFS_write(const char *path, const char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    // int retstat = 0;
    // log_msg("\nhradecFS_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
    //     path, buf, size, offset, fi
    //     );
    // // no need to get fpath on this one, since I work from fi->fh not the path
    // log_fi(fi);
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_write  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);

    return log_syscall("pwrite", pwrite(fi->fh, buf, size, offset), 0);
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int hradecFS_statfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_statfs  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
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

    return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
// this is a no-op in BBFS.  It just logs the call and returns success
int hradecFS_flush(const char *path, struct fuse_file_info *fi)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_flush  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    log_msg("\nhradecFS_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int hradecFS_release(const char *path, struct fuse_file_info *fi)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_release  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    // log_msg("\nhradecFS_release(path=\"%s\", fi=0x%08x)\n",
    //   path, fi);
    // log_fi(fi);

    // We need to close the file.  Had we allocated any resources
    // (buffers etc) we'd need to free them here as well.
    return log_syscall("close", close(fi->fh), 0);
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int hradecFS_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_fsync  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    log_msg("\nhradecFS_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
        path, datasync, fi);
    log_fi(fi);

    // some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
    if (datasync)
    return log_syscall("fdatasync", fdatasync(fi->fh), 0);
    else
#endif
    return log_syscall("fsync", fsync(fi->fh), 0);
}

#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int hradecFS_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_setxattr  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);
    char fpath[PATH_MAX];

    log_msg("\nhradecFS_setxattr(path=\"%s\", name=\"%s\", value=\"%s\", size=%d, flags=0x%08x)\n",
        path, name, value, size, flags);
    hradecFS_fullpath(fpath, path);

    return log_syscall("lsetxattr", lsetxattr(fpath, name, value, size, flags), 0);
}

/** Get extended attributes */
int hradecFS_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_getxattr  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);

    log_msg("\nhradecFS_getxattr(path = \"%s\", name = \"%s\", value = 0x%08x, size = %d)\n",
        path, name, value, size);
    hradecFS_fullpath(fpath, path);

    retstat = log_syscall("lgetxattr", lgetxattr(fpath, name, value, size), 0);
    if (retstat >= 0)
    log_msg("    value = \"%s\"\n", value);

    return retstat;
}

/** List extended attributes */
int hradecFS_listxattr(const char *path, char *list, size_t size)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    char *ptr;
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_listxattr  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);

    log_msg("hradecFS_listxattr(path=\"%s\", list=0x%08x, size=%d)\n",
        path, list, size
        );
    hradecFS_fullpath(fpath, path);

    retstat = log_syscall("llistxattr", llistxattr(fpath, list, size), 0);
    if (retstat >= 0) {
    log_msg("    returned attributes (length %d):\n", retstat);
    for (ptr = list; ptr < list + retstat; ptr += strlen(ptr)+1)
        log_msg("    \"%s\"\n", ptr);
    }

    return retstat;
}

/** Remove extended attributes */
int hradecFS_removexattr(const char *path, const char *name)
{
    char fpath[PATH_MAX];
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_removexattr  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);

    log_msg("\nhradecFS_removexattr(path=\"%s\", name=\"%s\")\n",
        path, name);
    hradecFS_fullpath(fpath, path);

    return log_syscall("lremovexattr", lremovexattr(fpath, name), 0);
}
#endif



/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ??? >>> I need to implement this...
int hradecFS_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_fsyncdir  %s\n>>>>>>>>>>>>>>>>>>>>>>>>>>\n", path);

    log_msg("\nhradecFS_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
        path, datasync, fi);
    log_fi(fi);

    return retstat;
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
// Undocumented but extraordinarily useful fact:  the fuse_context is
// set up before this function is called, and
// fuse_get_context()->private_data returns the user_data passed to
// fuse_main().  Really seems like either it should be a third
// parameter coming in here, or else the fact should be documented
// (and this might as well return void, as it did in older versions of
// FUSE).
void *hradecFS_init(struct fuse_conn_info *conn, fuse_config *fc)
{
    log_conn( conn );
    log_fuse_context( fuse_get_context() );
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_init  \n>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

    pthread_mutex_lock(&mutex);
    CACHE.cleanupBeforeStart();
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_init  \n>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    CACHE.cleanupCache();
    pthread_mutex_unlock(&mutex);

    // do the initial cache of the root folder
    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_init  \n>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    CACHE.doCachePathDir( "/" );

    log_msg("\n>>>>>>>>>>>>>>>>>>>>>>>>\n hradecFS_init  \n>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    // std::thread t1(task1, cleanupCache);

    return BB_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void hradecFS_destroy(void *userdata)
{
    log_msg( "\nhradecFS_destroy(userdata=0x%08x)\n", userdata );
}

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

    CACHE.init( path );

    // DIR_CACHE_INIT(path);
    // INIT_CACHE(path);
    //
    // CACHE_FILE_INIT(path);
    //
    // if( ! CACHE_FILE_EXISTS() ){
    //
    //
    // }

    if( CACHE.existsRemote( path ) ){

        retstat = access( CACHE.localPath( path ), mask );
    }

    // log_msg("\nhradecFS_access(path=\"%s\", mask=0%o, retstat=%d)\n",
    //     __path, mask, retstat);

    if (retstat < 0)
       retstat = log_error("hradecFS_access access");

    return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
// Not implemented.  I had a version that used creat() to create and
// open the file, which it turned out opened the file write-only.

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
int hradecFS_ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
    int retstat = 0;
    // CACHE.init( path );

    log_msg("\nhradecFS_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n", path, size, fi);
    log_fi(fi);

    int res;
    if (fi != NULL)
            res = ftruncate(fi->fh, size);
    else
            res = truncate(path, size);
    if (res == -1)
            return -errno;
    return 0;


    // retstat = ftruncate(fi->fh, offset);
    // if (retstat < 0)
    //    retstat = log_error("hradecFS_ftruncate ftruncate");
    //
    // return retstat;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int hradecFS_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int retstat = 0;

    log_msg("\nhradecFS_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n", path, statbuf, fi);
    log_fi(fi);

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

    return retstat;
}


struct hradecFS_oper_struc : fuse_operations  {
    hradecFS_oper_struc() {
        getattr = hradecFS_getattr;
        readlink = hradecFS_readlink;
        // no .getdir -- that's deprecated
        // getdir = NULL;
        mknod = hradecFS_mknod;
        mkdir = hradecFS_mkdir;
        unlink = hradecFS_unlink;
        rmdir = hradecFS_rmdir;
        symlink = hradecFS_symlink;
        rename = hradecFS_rename;
        link = hradecFS_link;
        chmod = hradecFS_chmod;
        chown = hradecFS_chown;
        truncate = hradecFS_truncate;
        // .utime = hradecFS_utime;
        open = hradecFS_open;
        read = hradecFS_read;
        write = hradecFS_write;
        /** Just a placeholder; don't set */ // huh???
        statfs = hradecFS_statfs;
        flush = hradecFS_flush;
        release = hradecFS_release;
        fsync = hradecFS_fsync;

        #ifdef HAVE_SYS_XATTR_H
        setxattr = hradecFS_setxattr;
        getxattr = hradecFS_getxattr;
        listxattr = hradecFS_listxattr;
        removexattr = hradecFS_removexattr;
        #endif

        opendir = hradecFS_opendir;
        readdir = hradecFS_readdir;
        releasedir = hradecFS_releasedir;
        fsyncdir = hradecFS_fsyncdir;
        init = hradecFS_init;
        destroy = hradecFS_destroy;
        access = hradecFS_access;
        truncate = hradecFS_ftruncate;
        // fgetattr = hradecFS_fgetattr;
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
