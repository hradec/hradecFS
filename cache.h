/*
  HradecFS
  Copyright (C) 2018 Roberto Hradec <me@hradec.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer".

*/



#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <cstdarg>
#include <thread>
#include <time.h>
#include <boost/algorithm/string/replace.hpp>
using namespace std;

#include "params.h"
#include "log.h"
#include "fileUtils.h"

#include "cache_utils.h"


int mkdir_p( char *path, mode_t mode=0777 ){
    long pos=0;
    int ret=0;
    char buff[1024];
    pthread_mutex_lock(&mutex);
    while( pos++ <= strlen(path) ){
        if( path[pos] == '/' ){
            strncpy ( buff, path, pos );
            buff[pos]=0;
            fprintf(stderr, "%s\n", buff);
            if( ! exists(buff) )
                ret = mkdir( buff, mode );
        }
    }
    fprintf(stderr, "%s\n", path);
    if( ! exists(path) )
        ret = mkdir( path, mode );
    pthread_mutex_unlock(&mutex);
    return ret;
}


class __cache {
        map <string, map<string, string> > m_cache;

        map <string, struct stat *> m_stat;

        // string cacheFileNotExist;
        // string cacheDirNotExist;
        // string basepath;
        // string basepathDir;
        // string cacheDir;
        // string cmd;

        string _db_path_ = "/dev/shm/bbfs.";
        string _cache_root_;
        string _cache_path_;
        string _cache_control_;


        string _cachePathBase(){
            // if ( ! exists( BB_DATA->mountdir ) )
            //     mkdir_p( BB_DATA->mountdir, 0777 );
            // if ( ! exists( BB_DATA->cachedir ) )
            //     mkdir_p( BB_DATA->cachedir, 0777 );
            _cache_root_ = BB_DATA->cachedir;
            return _cache_root_;
        }
        string _cachePath(){
            _cache_path_ = _cachePathBase() + "/filesystem";
            if ( ! exists( _cache_path_.c_str() ) )
                mkdir( _cache_path_.c_str(), 0777 );
            return _cache_path_;
        }
        string _cowPath(){
            _cache_path_ = _cachePathBase() + "/cow";
            if ( ! exists( _cache_path_.c_str() ) )
                mkdir( _cache_path_.c_str(), 0777 );
            return _cache_path_;
        }
        string _cacheControl(){
            _cache_control_ = _cachePathBase() + "/control/";
            if ( ! exists( _cache_control_.c_str() ) )
                mkdir( _cache_control_.c_str(), 0777 );
            return _cache_control_;
        }

    public:
        init(string path){
            std::map<string, map<string, string> >::iterator it;

            // check if we already have it
            it = m_cache.find(path);
            if ( it != m_cache.end() ){
                return;
            }

            string path2 = boost::replace_all_copy( path, "//" , "/" );

            m_cache[path]["cacheFileNotExist"]  = _format( "%s%s.cacheFileNotExist", _cacheControl().c_str(), boost::replace_all_copy( path2, "/", "_" ).c_str() );
            m_cache[path]["cacheDirNotExist"]   = _format( "%s%s.cacheDirNotExist", _cacheControl().c_str(), boost::replace_all_copy( rtrim( _dirname( path2 ),"/" ), "/" , "_" ).c_str() );
            m_cache[path]["cacheLockFile"]      = _format( "%s%s.cacheLockFile",  _cacheControl().c_str(), boost::replace_all_copy( path2, "/", "_" ).c_str() );
            m_cache[path]["cacheReadDir"]       = _format( "%s%s.cacheReadDir", _cacheControl().c_str(), boost::replace_all_copy( rtrim(path2,"/"), "/" , "_" ).c_str() );
            m_cache[path]["cacheFile"]          = _cachePath() + path2;
            m_cache[path]["cacheFileDir"]       = _cachePath() + _dirname( path2 );
            m_cache[path]["abspath"]            = string(BB_DATA->rootdir) + "/" + path2;
            m_cache[path]["cowFile"]            = _cowPath() + path2;
            m_stat[path]                        = NULL;

            m_cache[path]["cacheFile"]          = boost::replace_all_copy( m_cache[path]["cacheFile"], "//" , "/" );
            m_cache[path]["cacheFileDir"]       = boost::replace_all_copy( m_cache[path]["cacheFileDir"], "//" , "/" );
            m_cache[path]["abspath"]            = boost::replace_all_copy( m_cache[path]["abspath"], "//" , "/" );

            // m_cache[path]["cacheFile_log"]      = m_cache[path]["cacheFile"] + ".bbfslog";
            m_cache[path]["cacheFile_log"]      = _format( "%s%s.bbfslog",  _cacheControl().c_str(), boost::replace_all_copy( path2, "/", "_" ).c_str() );

        }

        void cleanupBeforeStart(){
            // remove all leftover lockFiles, if any!
            system( (string("rm -rf ")+_cacheControl()+"/*.cacheLockFile").c_str() );
        }

        void cleanupCache(){
            string cmd = "find "+_cacheControl()+" -ctime -60  -name '*cacheReadDir' -exec rm {} \\; ";
            system( cmd.c_str() );
            log_msg("cleanupCache %s", cmd.c_str());
            // struct stat fileAttrs;
            // vector<string>  list;
            // vector<string>  __list;
            // long id;
            // std::time_t t = std::time(0);
            // list = glob( _cacheControl()+"*cacheReadDir" );
            // __list = glob( _cacheControl()+".cacheReadDir" );
            // for (id=0 ; id<__list.size(); ++id){
            //     list.push_back( __list[id] );
            // }
            // for (id=0 ; id<list.size(); ++id){
            //     // get size
            //     if( stat(list[id].c_str(), &fileAttrs) != 0 )
            //         log_msg("cleanupCache error");
            //     log_msg("cleanupCache: %s %i - %i\n", list[id].c_str(), fileAttrs.st_ctime, t);
            //     if( t-fileAttrs.st_ctime > 60*10 ){
            //         log_msg("cleanupCache: %s %i\n", list[id].c_str(), t-fileAttrs.st_ctime);
            //     }
            // }
        }

        int stat(string path, struct stat *statbuf){
            // get path STAT and cache it.
            int retstat;
            // if(m_stat[path]==NULL){
                if( existsLocal(path) ){
                    retstat = lstat( localPath(path), statbuf );
                    // log_msg("..%s..%d..\n", localPath(path), statbuf);
                }else if( existsRemote( path ) ){
                    retstat = lstat( remotePath(path), statbuf );
                    log_msg("..%s..%d..\n", remotePath(path), statbuf);
                }else{
                    statbuf=NULL;
                }
                m_stat[path]=statbuf;
            // }else{
            //     statbuf->st_dev     = m_stat[path]->st_dev     ;
            //     statbuf->st_ino     = m_stat[path]->st_ino     ;
            //     statbuf->st_mode    = m_stat[path]->st_mode    ;
            //     statbuf->st_nlink   = m_stat[path]->st_nlink   ;
            //     statbuf->st_uid     = m_stat[path]->st_uid     ;
            //     statbuf->st_gid     = m_stat[path]->st_gid     ;
            //     statbuf->st_rdev    = m_stat[path]->st_rdev    ;
            //     statbuf->st_size    = m_stat[path]->st_size    ;
            //     statbuf->st_blksize = m_stat[path]->st_blksize ;
            //     statbuf->st_blocks  = m_stat[path]->st_blocks  ;
            //     statbuf->st_atime   = m_stat[path]->st_atime   ;
            //     statbuf->st_mtime   = m_stat[path]->st_mtime   ;
            //     statbuf->st_ctime   = m_stat[path]->st_ctime   ;
            // }

            return m_stat[path]==NULL ? -2 : 0;
        }

        char * localPathLock(string path){
            this->init( path );
            return m_cache[path]["cacheLockFile"].c_str();
        }
        char * localPathLog(string path){
            this->init( path );
            return m_cache[path]["cacheFile_log"].c_str();
        }
        char * localPath(string path){
            this->init( path );
            return m_cache[path]["cacheFile"].c_str();
        }
        char * localPathDir(string path){
            this->init( path );
            return m_cache[path]["cacheFileDir"].c_str();
        }
        char * remotePath(string path){
            this->init( path );
            return m_cache[path]["abspath"].c_str();
        }

        refresh(string path){
            this->init( path );
            m_cache[path]["stat"] = "";
        }

        localFileNotExist(string path, bool _remove=false){
            this->init( path );
            log_msg("REMOTE      cacheFileNotExist: %s\n", m_cache[path]["cacheFileNotExist"].c_str());
            if(_remove){
                remove( m_cache[path]["cacheFileNotExist"].c_str() );
            }else{
                creat( m_cache[path]["cacheFileNotExist"].c_str(), 0777);
            }
        }
        localFileNotExistRemove(string path){
            this->init( path );
            localFileNotExist( path, true );
        }

        localDirNotExist(string path){
            this->init( path );
            log_msg("REMOTE      cacheDirNotExist: %s %s\n", path.c_str(), m_cache[path]["cacheDirNotExist"].c_str());
            creat( m_cache[path]["cacheDirNotExist"].c_str(), 0777 );
        }

        readDirCached(string path){
            this->init( path );
            log_msg("REMOTE      cacheReadDir: %s\n", m_cache[path]["cacheReadDir"].c_str());
            creat( m_cache[path]["cacheReadDir"].c_str(), 0777 );
        }

        doCachePath(string path, struct stat *statbuf=NULL, int __depth=0){
            // does a initial cache of folder, files and links, to avoid
            // querying the remote side for simple readdir() calls to the same path
            // TODO: a refresh thread to maintain this initial cache up2date with the remote side!

            char buf[8192];
            unsigned long len;

            // we dont need to check the return of lstat, since we KNOWN at this point that
            // the path exists!
            log_msg("1.doCachePath %s", remotePath(path) );
            lstat(remotePath(path), statbuf);


            if(islnk(remotePath(path))){
                if ((len = readlink(remotePath(path), buf, sizeof(buf)-1)) != -1){
                    buf[len] = '\0';
                    symlink(buf, localPath(path));
                    log_msg("\nlink path %s = remotePath(%s) / localPath(%s)\n", buf, remotePath(path), localPath(path));
                }
            }else if( isfile(remotePath(path)) ){
                creat( localPath(path), statbuf->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
                creat( m_cache[path]["cacheFile_log"].c_str(), statbuf->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
                log_msg("\n create skeleton %s = %s\n", localPath(path),  m_cache[path]["cacheFile_log"].c_str());
            }else if( isdir(remotePath(path)) ){
                log_msg("\n\nmkdir: %s\n", localPath(path));
                mkdir( localPath(path), statbuf->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
                /// if a folder cache it, but only 1 recusion level!
                //if(__depth<=1){
                //     doCachePathDir(path, __depth+1);
                //}
            }else{
                mkdir( localPath(path), statbuf->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
            }

            if(! islnk(remotePath(path))){
                chown( localPath(path), statbuf->st_uid, statbuf->st_gid );

                struct utimbuf times;
                times.actime = statbuf->st_atime;
                times.modtime = statbuf->st_mtime;
                utime( localPath(path), &times );
            }

            // log_msg("\nchmod %s = %o\n", remotePath(path), statbuf->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
        }

        doCachePathDir(string path, int __depth=0){
            // do a initial cache of the entire folder of a path, so we can
            // collect all the data we can using the same time of a single path.

            // if already cached, don't cache again!

            if ( isDirCached(path) ){
                return ;
            }

            vector<string> files = vector<string>();
            getdir( string(remotePath(path))+"/", files );

            for (unsigned int i = 0;i < files.size();i++) {
                struct stat statbuf2;
                string glob_path = boost::replace_all_copy( files[i], BB_DATA->rootdir, "" );
                log_msg("----------->%s\n", files[i].c_str());
                if( glob_path != path ){
                    init( glob_path );
                }
                if( ! existsLocal( glob_path ) ){
                    doCachePath( glob_path, &statbuf2, __depth );
                }

            }
            // set dir as cached!!
            readDirCached( path );
            log_msg("-->%s\n", path.c_str());
        }

        doCachePathParentDir(string path){
            // cache the parent folder of a path!

            string parent=_dirname(path);
            fprintf(stderr, "\ndoCachePathParentDir\n");
            log_msg("doCachePathParentDir(%s) -> %s / %s\n", path.c_str(), parent.c_str(), BB_DATA->rootdir);
            if ( parent != BB_DATA->rootdir ) {
                init( parent );
                doCachePathDir( parent );
            }
        }

        bool isDirCached(string path){
            if ( exists( m_cache[path]["cacheReadDir"].c_str() ) )
                return true;
            return false;
        }

        bool existsRemote(string path, bool checkLocal=true){

            // if the file or the folder the files is in doenst exist in the remote filesystem, return NotExist
            // this is a HUGE speedup for searchpaths, like PYTHONPATH!!!
            // string __dir=_dirname(path);
            // while( __dir == "/" ){
            // }
            if ( exists( m_cache[path]["cacheDirNotExist"].c_str() ) || exists( m_cache[path]["cacheFileNotExist"].c_str() ) ){
                log_msg("\nexistsRemote -> first if - return False - '%s'  '%s'\n",m_cache[path]["cacheDirNotExist"].c_str(), m_cache[path]["cacheFileNotExist"].c_str() );
                return false;
            }

            // we need turn this  on/off for files that only exist locally (fileInSync)
            if(checkLocal){
                // if dir is cached or exists locally, means it exists remotely!
                // if it was deleted remotely, theres another thread wich will delete it locally,
                // so we don't have to check remotely here!!
                if ( existsLocal(path) ){
                    if( isdir(path.c_str()) )
                        return true;
                    bool ret = exists( m_cache[path]["cacheFile_log"].c_str() );
                    log_msg("\nexistsRemote -> second if - return %d\n",ret );
                    return ret;
                }
            }

            // if file doesn't exist locally, but its folder has being cached,
            // it means the file doesn't exist remotely either, so we don't have to
            // query remotely!!
            // if it shows up remotely, the refresh thread should make it appears locally,
            // so the previous if will make it return true!
            if ( isDirCached( _dirname(path) ) ){
                if(checkLocal){
                    // if the cachefile log file exists, means the file exists remotely, But
                    // was deleted locally in the cache only, so in this case, we
                    // should retrieve it again!
                    pthread_mutex_lock(&mutex);
                    if( exists( m_cache[path]["cacheFile_log"].c_str() ) ){
                        log_msg("\nexistsRemote -> third if log exist - %s\n", m_cache[path]["cacheFile_log"].c_str() );
                        struct stat statbuf;
                        lstat(m_cache[path]["cacheFile_log"].c_str(), &statbuf);
                        remove( m_cache[path]["cacheFile_log"].c_str() );
                        creat( m_cache[path]["cacheFile_log"].c_str(), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
                        creat( localPath(path), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
                        pthread_mutex_unlock(&mutex);
                        return true;
                    }
                    pthread_mutex_unlock(&mutex);
                }
                log_msg("\nexistsRemote -> third if - return False\n" );
                return false;
            }

            // Now that we exausted the local checks, there's no option but go to the remote
            // filesystem. But we MUST cache the lookup result so we don't wast time in the future!

            // first, check if the remote folder exists. If not, we already known the files in this folder
            // will not exist localy. So just cache the folder and return not_exist!!!!
            if ( ! exists( _dirname( remotePath(path) ).c_str() ) ){
                log_msg("existsRemote -> quarth if - return False\n" );
                localDirNotExist(path);
                return false;
            }else{
                // if the folder exists remotely,
                // lets cache it to avoid future remote query on the same path
                doCachePathParentDir( path );

                // call itself again to do the local evaluation again after the caching!!
                log_msg("existsRemote -> fifth if  - recurse\n" );
                return existsRemote( path );
            }

            // if path doesn't exist in the remote filesystem, return notExist!
            // also set local cache of the remote state!
            if ( ! exists( remotePath(path) ) ){
                log_msg("existsRemote -> sixth if  - don't exist\n" );

                // since we're accessing the remote filesystem, check if the folder of the path exists
                if( ! existsSymLink( m_cache[path]["cacheFileDir"].c_str() ) ){
                    localDirNotExist(path);
                }else{
                    localFileNotExist(path);
                }
                return false;
            }

            return true;
        }
        bool existsLocal(string path){

            // if the file or the folder the files is in doenst exist in the remote filesystem, return NotExist
            // this is a HUGE speedup for searchpaths, like PYTHONPATH!!!
            // string __dir=_dirname(path);
            // while( __dir == "/" ){
            // }
            if ( exists( m_cache[path]["cacheDirNotExist"].c_str() ) || exists( m_cache[path]["cacheFileNotExist"].c_str() ) ){
                log_msg("existsLocal -> first if - return False - '%s' '%s'  '%s'\n",path.c_str(), m_cache[path]["cacheDirNotExist"].c_str(), m_cache[path]["cacheFileNotExist"].c_str() );
                return false;
            }

            // if the dir of the folder of the path doesn't exist locally, return Not Exists already..
            // don't even check for file!
            if ( ! exists( localPathDir(path) ) )
                return false;

            // if the files exists in the cache filesystem, return EXISTS!
            if ( exists( localPath(path) ) )
                return true;

            // else, NOT EXIST!
            return false;
        }

        bool fileNotExist(){};

        bool removeFile( string path ){
            this->init( path );
            struct stat statbuf;
            stat(path, &statbuf);
            creat( m_cache[path]["cacheFileNotExist"].c_str(), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
        }

        bool removeDir( string path ){
            this->init( path );
            struct stat statbuf;
            stat(path, &statbuf);
            creat( m_cache[path]["cacheDirNotExist"].c_str(), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
        }

        bool fileInSync( string path ){
            // get size
            long localSize = 0;
            long remoteSize = 0;
            struct stat cachefile_st;
            log_msg("----------------------\n%s\n---------------------", path.c_str());
            if( this->existsLocal(path) ){
                localSize = getFileSize(this->localPath(path));
                log_msg("\nfileInSync exist locally - %s  %s - %ld\n", this->localPath(path), m_cache[path]["cacheFileNotExist"].c_str(), localSize );
                if( localSize == 0 ){
                    log_msg("\nbut size is 0, so its just an skeleton! (need rsync)\n" );
                    return false;
                }else{
                    // this account for locally created files that don't exist remotely! (COW)
                    if( ! this->existsRemote(path, false) ){
                        log_msg("\nbut size is NOT 0, and it doesn;t exist remotely!\n" );
                        return true;
                    }
                }

            }
            log_msg("\nfileInSync check size of local against remote()\n" );

            // get size
            struct stat fpath_st;
            stat((const char *)this->remotePath(path), &fpath_st);
            remoteSize = getFileSize(this->remotePath(path));


            log_msg( "\nfileInSync: %s size: %lld \n", this->remotePath(path), localSize );
            log_msg( "\nfileInSync: %s size: %lld \n", this->localPath(path), remoteSize );

            // check if the 2 sizes are the same or not!
            return localSize == remoteSize;
        }

        bool openFiles( string path, struct fuse_file_info *fi ){

        };
};
