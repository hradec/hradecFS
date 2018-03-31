/*
  HradecFS
  Copyright (C) 2018 Roberto Hradec <me@hradec.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer".

*/



#include <iostream>
#include <cstdarg>
#include <thread>
#include <time.h>
#include <boost/algorithm/string/replace.hpp>

#include "params.h"
#include "log.h"
#include "fileUtils.h"

#include "cache_utils.h"

#include <map>
#include <vector>
#include <string>
using namespace std;


static pthread_mutex_t mutex_readlink;
static pthread_mutex_t mutex_mkdir_p;
static pthread_mutex_t mutex_cache_path;
static pthread_mutex_t mutex_cache_path_dir;
static pthread_mutex_t mutex_localFileExist;



int mkdir_p( char *path, mode_t mode=0777 ){
    long pos=0;
    int ret=0;
    char buff[1024];
    pthread_mutex_lock(&mutex_mkdir_p);
    while( pos++ <= strlen(path) ){
        if( path[pos] == '/' ){
            strncpy ( buff, path, pos );
            buff[pos]=0;
            // fprintf(stderr, "%s\n", buff);
            if( ! exists(buff) )
                ret = mkdir( buff, mode );
        }
    }
    // fprintf(stderr, "%s\n", path);
    if( ! exists(path) )
        ret = mkdir( path, mode );
    pthread_mutex_unlock(&mutex_mkdir_p);
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
            return string(_cache_root_);
        }
        string _cachePath(){
            _cache_path_ = _cachePathBase() + "/filesystem";
            if ( ! exists( _cache_path_.c_str() ) )
                mkdir( _cache_path_.c_str(), 0777 );
            return string(_cache_path_);
        }
        string _cowPath(){
            _cache_path_ = _cachePathBase() + "/cow";
            if ( ! exists( _cache_path_.c_str() ) )
                mkdir( _cache_path_.c_str(), 0777 );
            return string(_cache_path_);
        }
        string _cacheControl(){
            _cache_control_ = _cachePathBase() + "/control/";
            if ( ! exists( _cache_control_.c_str() ) )
                mkdir( _cache_control_.c_str(), 0777 );
            return string(_cache_control_);
        }

    public:
        string fixPath(string path){
            if( path.length() > 1 ){
                path = rtrim( path, "/" );
            }else{
                if( path == "" ) path = "/";
            }
            return boost::replace_all_copy( path, "//" , "/" );
        }

        void init(string path){
            string __sep__ = "_";

            string path2 = path;
            if( path.length() > 1 )
                path2 = boost::replace_all_copy(  path, "//" , "/" );

            // check if we already have i
            pthread_mutex_lock(&mutex_localFileExist);
            if ( int( m_cache.count( path ) ) > 0 ) {
                // log_msg("=============>init(%s)  return because we already have it initialized! m_cache.size = %d<<<<<<\n",path.c_str(), m_cache.size() );
            }else{
                log_msg("\n==== init(%s)\n", path.c_str());
                // if( path2 == "/" ) path2 = __sep__;

                m_cache[path]["cacheFileNotExist"]  = _format( "%s%s.cacheFileNotExist", _cacheControl().c_str(), boost::replace_all_copy( path2, "/", __sep__ ).c_str() );
                m_cache[path]["cacheDirNotExist"]   = _format( "%s%s.cacheDirNotExist", _cacheControl().c_str(), boost::replace_all_copy( rtrim( _dirname( path2 ),"/" ), "/" , __sep__ ).c_str() );
                m_cache[path]["cacheLockFile"]      = _format( "%s%s.cacheLockFile",  _cacheControl().c_str(), boost::replace_all_copy( path2, "/", __sep__ ).c_str() );
                m_cache[path]["cacheReadDir"]       = _format( "%s%s.cacheReadDir", _cacheControl().c_str(), boost::replace_all_copy( rtrim(path2,"/"), "/" , __sep__ ).c_str() );
                m_cache[path]["cacheFile"]          = _format( "%s%s", _cachePath().c_str(), path2.c_str() );
                m_cache[path]["cacheFileDir"]       = _format( "%s%s", _cachePath().c_str(), _dirname( path2 ).c_str() );
                m_cache[path]["abspath"]            =  boost::replace_all_copy( string( BB_DATA->rootdir ) + "/" + path2, "//" , "/" );
                m_cache[path]["cowFile"]            = _cowPath() + path2;

                m_cache[path]["cacheFile"]          = boost::replace_all_copy( m_cache[path]["cacheFile"], "//" , "/" );
                m_cache[path]["cacheFileDir"]       = boost::replace_all_copy( m_cache[path]["cacheFileDir"], "//" , "/" );
                m_cache[path]["abspath"]            = boost::replace_all_copy( m_cache[path]["abspath"], "//" , "/" );

                m_cache[path]["cacheFile_log"]      = _format( "%s%s.bbfslog",  _cacheControl().c_str(), boost::replace_all_copy( path2, "/", __sep__ ).c_str() );

                m_stat[path]                        = NULL;
                log_msg("------------->%s....%s....%d<<<<<<\n",path.c_str(), m_cache[path]["cacheFile_log"].c_str(), m_cache.count(path));
            }
            pthread_mutex_unlock(&mutex_localFileExist);
        }

        void cleanupBeforeStart(){
            // remove all leftover lockFiles, if any!
            remove( ( string( _cacheControl() ) + "/.cacheReadDir" ).c_str() );

            vector<string> files = glob(_cacheControl() + "*cacheLockFile" );
            for ( unsigned int i = 0; i < files.size(); i++ ) {
                remove( files[i].c_str() );
            }

            vector<string> files2 = glob(_cacheControl() + "*cacheReadDir" );
            for ( unsigned int i = 0; i < files2.size(); i++ ) {
                remove( files2[i].c_str() );
            }

            vector<string> files3 = glob(_cacheControl() + "*__folder__" );
            for ( unsigned int i = 0; i < files3.size(); i++ ) {
                remove( files3[i].c_str() );
            }

            vector<string> files4 = glob(_cacheControl() + "*__link__" );
            for ( unsigned int i = 0; i < files4.size(); i++ ) {
                remove( files4[i].c_str() );
            }

            // account for local created files!!
            files4 = glob(_cacheControl() + "*__local__" );
            for ( unsigned int i = 0; i < files4.size(); i++ ) {
                string path = boost::replace_all_copy( getPathFromLogFile( files4[i] ), _cachePath(), "" );
                init( path );
                m_cache[path]["cacheFile_log"] = files4[i];
                log_msg("------------->%s....%s....%d<<<<<<\n",path.c_str(), m_cache[path]["cacheFile_log"].c_str(), m_cache.count(path));
            }

            files4 = glob(_cacheControl() + "*__link_local__" );
            for ( unsigned int i = 0; i < files4.size(); i++ ) {
                string path = boost::replace_all_copy( getPathFromLogFile( files4[i] ), _cachePath(), "" );
                init( path );
                m_cache[path]["cacheFile_log"] = files4[i];
                log_msg("------------->%s....%s....%d<<<<<<\n",path.c_str(), m_cache[path]["cacheFile_log"].c_str(), m_cache.count(path));
            }

            files4 = glob(_cacheControl() + "*__folder_local__" );
            for ( unsigned int i = 0; i < files4.size(); i++ ) {
                string path = boost::replace_all_copy( getPathFromLogFile( files4[i] ), _cachePath(), "" );
                init( path );
                m_cache[path]["cacheFile_log"] = files4[i];
                log_msg("------------->%s....%s....%d<<<<<<\n",path.c_str(), m_cache[path]["cacheFile_log"].c_str(), m_cache.count(path));
            }

        }

        void cleanupCache(){
            string cmd = "find "+_cacheControl()+" -ctime -60  -name '*cacheReadDir' -exec rm {} \\; &";
            // system( cmd.c_str() );
            log_msg("cleanupCache %s", cmd.c_str());
        }

        int stat(string path){
            struct stat statbuf;
            stat( path, &statbuf );
        }

        int stat(string path, struct stat *statbuf){
            // get path STAT and cache it.
            int retstat=-2;
            log_msg("    >>>  CACHE.stat(%s)\n", path.c_str());
            // if(m_stat[path]==NULL){
                if( existsLocal(path) ){
                    retstat = lstat( localPath(path), statbuf );
                    statbuf->st_size = getPathSizeFromLog( path );
                }else if( existsRemote( path ) ){
                    retstat = lstat( remotePath(path), statbuf );
                    log_msg("..%s..%d..\n", remotePath(path), statbuf);
                }else{
                    statbuf=NULL;
                }

                if( statbuf != NULL && retstat < 0 ){
                    memset( statbuf, 0, sizeof(statbuf) );
                }

                // m_stat[path]=new struct stat;
            //     m_stat[path]->st_dev     = statbuf->st_dev;
            //     m_stat[path]->st_ino     = statbuf->st_ino;
            //     m_stat[path]->st_mode    = statbuf->st_mode;
            //     m_stat[path]->st_nlink   = statbuf->st_nlink;
            //     m_stat[path]->st_uid     = statbuf->st_uid;
            //     m_stat[path]->st_gid     = statbuf->st_gid;
            //     m_stat[path]->st_rdev    = statbuf->st_rdev;
            //     m_stat[path]->st_size    = statbuf->st_size;
            //     m_stat[path]->st_blksize = statbuf->st_blksize;
            //     m_stat[path]->st_blocks  = statbuf->st_blocks;
            //     m_stat[path]->st_atime   = statbuf->st_atime;
            //     m_stat[path]->st_mtime   = statbuf->st_mtime;
            //     m_stat[path]->st_ctime   = statbuf->st_ctime;
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

            // log_msg("CACHE.stat(%s)..%d..\n", path.c_str(), int(m_stat[path]->st_ino));
            log_msg( "    >>>  CACHE.stat(%s) return %d \n", path.c_str(), retstat );
            return retstat;
        }

        long long getPathSizeFromLog( string path ){
            return  getPathSizeFromLogFile( localPathLog( path ) );
        }
        long long getPathSizeFromLogFile( string logfilepath ){
            char buff[8193];
            int nsplit;
            int file = open( logfilepath.c_str(), O_RDONLY );
            read( file, buff, 8192 );
            close( file );
            char** lines = str_split( buff, '\n', &nsplit );
            char** split = str_split( lines[0], '*', &nsplit );
            if( nsplit>0 ){
                return (long long)atoll( split[1] );
            }
            return 0;
        }

        string getPathFromLog( string path ){
            return  getPathFromLogFile( localPathLog( path ) );
        }
        string getPathFromLogFile( string logfilepath ){
            char buff[8193];
            int nsplit;
            int file = open( logfilepath.c_str(), O_RDONLY );
            read( file, buff, 8192 );
            close( file );
            char** lines = str_split( buff, '\n', &nsplit );
            char** split = str_split( lines[0], '*', &nsplit );
            if( nsplit>0 ){
                return string(split[0]);
            }
            return "error";
        }

        const char * localPathLock(string path){
            init( path );
            return m_cache[path]["cacheLockFile"].c_str();
        }
        const char * localPathLog(string path, string suffix=""){
            init( path );
            if( suffix != "" ){
                if ( int( m_cache[path]["cacheFile_log"].find(suffix) ) < 0 )
                    m_cache[path]["cacheFile_log"] += suffix;
            }
            return m_cache[path]["cacheFile_log"].c_str();
        }
        const char * localPath(string path){
            init( path );
            return m_cache[path]["cacheFile"].c_str();
        }
        const char * localPathDir(string path){
            init( path );
            return m_cache[path]["cacheFileDir"].c_str();
        }
        const char * remotePath(string path){
            init( path );
            return m_cache[path]["abspath"].c_str();
        }

        void refresh(string path){
            init( path );
            m_cache[path]["stat"] = "";
        }

        void localFileNotExist(string path, bool _remove=false){
            const char *ft="cacheFileNotExist";
            // if( isdir( path.c_str() ) ){
            //     ft="cacheDirNotExist";
            // }
            log_msg("   localFileNotExist   %s: %s\n", ft, m_cache[path]["cacheFileNotExist"].c_str());
            if(_remove){
                if( exists( m_cache[path][ft].c_str() ) )
                    remove( m_cache[path][ft].c_str() );
            }else{
                creat( m_cache[path][ft].c_str(), 0777 );
            }
        }
        void localFileNotExistRemove(string path){
            localFileNotExist( path, true );
        }
        void localDirNotExist(string path, bool _remove=false){
            localFileNotExist(path, _remove);
        }
        void localDirNotExistRemove(string path){
            localFileNotExist(path, true);
        }

        void readDirCached(string path){
            init( path );
            log_msg("   readDirCached   cacheReadDir: %s\n", m_cache[path]["cacheReadDir"].c_str());
            creat( m_cache[path]["cacheReadDir"].c_str(), 0777 );
            if ( m_cache[path]["cacheFile_log"].find(".__folder__") == string::npos )
                m_cache[path]["cacheFile_log"] += ".__folder__";
            localFileExist(path);
        }

        void setStats(string path, struct stat *statbuf){
            // set all other relevant stats to file/folders!
            // the path is an absolute path, not a index path!
            // this way, we can use this function for localPath() and localPathLog()
            if( ! islnk( path.c_str() ) ){
                chown( path.c_str(), statbuf->st_uid, statbuf->st_gid );
                chmod( path.c_str(), statbuf->st_mode );
                struct utimbuf times;
                times.actime  = statbuf->st_atime;
                times.modtime = statbuf->st_mtime;
                utime( path.c_str(), &times );
            }
        }

        void localFileExist(string path){
            string linkSuffix   = ".__link__";
            string fileSuffix   = "";
            string folderSuffix = ".__folder__";
            bool localFile = int( m_cache[path]["cacheFile_log"].find(".__local__") ) >= 0;
            struct stat statbuf;
            memset( &statbuf, 0, sizeof(statbuf) );
            if ( localFile || lstat( remotePath(path), &statbuf ) != 0 ) {
                if ( lstat( localPath(path), &statbuf ) != 0 ) {
                    // if no stat, the file don't exist remotely, so it's a local only file!
                    // we set a standard umask for now!
                    statbuf.st_mode = S_IRUSR | S_IWUSR | S_IXUSR |
                                      S_IRGRP | S_IWGRP | S_IXGRP |
                                      S_IROTH | S_IWOTH | S_IXOTH ;
                    statbuf.st_uid   = 0;
                    statbuf.st_gid   = 0;
                    statbuf.st_atime = time(NULL);
                    statbuf.st_mtime = time(NULL);
                    statbuf.st_ctime = time(NULL);
                    // and we set special log file suffixes, so they don't
                    // disapear when the cache fs is remounted
                }
                linkSuffix   = ".__link_local__";
                folderSuffix = ".__folder_local__";
                fileSuffix   = ".__local__";
            };


            // pthread_mutex_lock(&mutex_localFileExist);
            if ( ! exists( localPath( path ) ) ) {
                creat( localPath( path ), statbuf.st_mode & ( S_IRWXU | S_IRWXG | S_IRWXO ) );
            }

            // set remote file stats to file
            setStats( localPath( path ), &statbuf );

            // fix log file
            if ( islnk( localPath( path ) ) ){
                if ( int( m_cache[path]["cacheFile_log"].find(linkSuffix) ) < 0 )
                    m_cache[path]["cacheFile_log"] += linkSuffix;
            }else if ( isdir( localPath( path ) ) ){
                if ( int( m_cache[path]["cacheFile_log"].find(folderSuffix) ) < 0 )
                    m_cache[path]["cacheFile_log"] = m_cache[path]["cacheFile_log"] + folderSuffix;
            }else if ( fileSuffix != "" ) {
                if ( int( m_cache[path]["cacheFile_log"].find(fileSuffix) ) < 0 )
                    m_cache[path]["cacheFile_log"] = m_cache[path]["cacheFile_log"] + fileSuffix;
            }

            // create a log file for every file/folder/link
            // we use the log file to known what's the remote side and what's not!
            int file = creat( localPathLog( path ), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
            string tmp = _format( "%s*%lld\n", localPath( path ) , (long long) statbuf.st_size );
            write( file, tmp.c_str(),  tmp.length() );
            close( file );
            localFileNotExistRemove( path );

            // set remote file stats to log file as well
            setStats( localPathLog( path ), &statbuf);

            log_msg( "\n >>> create skeleton %s --> %s \n",  localPathLog( path ), localPath( path ) );
            // pthread_mutex_unlock(&mutex_localFileExist);
        }

        void doCachePath(string path, int __depth=0){
            struct stat statbuf;
            doCachePath( path, &statbuf, __depth);
        }
        void doCachePath(string path, struct stat *statbuf, int __depth=0){
            // does a initial cache of folder, files and links, to avoid
            // querying the remote side for simple readdir() calls to the same path
            // TODO: a refresh thread to maintain this initial cache up2date with the remote side!
            init(path);
            char buf[8192];
            unsigned long len;

            path = fixPath(path);

            // we dont need to check the return of lstat, since we KNOWN at this point that
            // the path exists!
            log_msg("1.doCachePath %s", remotePath(path) );
            lstat(remotePath(path), statbuf);

            if ( islnk( remotePath(path) ) ){
                if ( (len = readlink(remotePath(path), buf, sizeof(buf)-1)) != -1 ){
                    buf[len] = '\0';
                    symlink(buf, localPath(path));
                    log_msg( "\nlink path %s = remotePath(%s) %s \n", buf, remotePath(path), localPath(path) );
                }
            }else
            if( isfile( remotePath(path) ) ){
                // we don't need to do anything here, since
                // localFileExist() does the creation of normal file skeletons.

            }else if( isdir( remotePath(path) ) ){
                log_msg( "\n\nmkdir: %s  %d \n", localPath(path),
                    mkdir( localPath(path), statbuf->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) ) );

                log_msg( "\n\nmkdir: %s  \n", m_cache[path]["cacheFile_log"].c_str());
                // localFileNotExistRemove(path);
                /// if a folder cache it, but only 1 recusion level!
                //if(__depth<=1){
                //     doCachePathDir(path, __depth+1);
                //}
            }else{
                // mkdir( localPath(path), statbuf->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
                // symlink("NOT_DIR_FILE_OR_LINK_REMOTELY", localPath(path));
                localFileNotExist( path );
                return;
            }
            localFileExist(path);
        }

        void doCachePathDir(string path, int __depth=0){
            // do a initial cache of the entire folder of a path, so we can
            // collect all the data we can using the same time of a single path.
            init(path);
            log_msg( "====> doCachePathDir: %s -> %s\n", path.c_str(), remotePath(path) );

            // if already cached, don't cache again!
            path = fixPath(path);
            if ( isDirCached( path ) ){
                log_msg( "======> doCachePathDir: isDirCached True - %s\n", path.c_str() );
                return ;
            }

            // log_msg( "-----------=====>%s<===>%s<===\n", __remotePath.c_str(), path.c_str() );

            // vector<string> localFiles = vector<string>();
            // getdir( string( localPath(path) ) + "/", localFiles );
            // for ( unsigned int i = 0; i < localFiles.size(); i++ ) {
            //     string glob_path = boost::replace_all_copy( localFiles[i], _cachePath(), "" );
            //     log_msg("================>%s\n", glob_path.c_str());
            //     localFileNotExist( glob_path );
            // }
            doCachePath(path);

            vector<string> files = vector<string>();
            getdir( string( remotePath(path) ) + "/", files );

            for ( unsigned int i = 0; i < files.size(); i++ ) {
                struct stat statbuf2;
                string glob_path = boost::replace_all_copy( files[i], BB_DATA->rootdir, "" );
                log_msg("---getdir-------->%s\n", files[i].c_str());
                if( glob_path != path ) init( glob_path );
                if( ! existsLocal( glob_path ) ){
                    doCachePath( glob_path, &statbuf2, __depth );
                }
            }

            // TODO: set bbfslog files that are not in the folder anymore as dontExist
            // getdir( string(localPath(path))+"/*log", files );
            // for (unsigned int i = 0;i < files.size();i++) {
            //     localFileNotExist(  )
            // }

            // set dir as cached!!
            readDirCached( path );
            // log_msg("-->%s\n", path.c_str());

        }

        void doCachePathParentDir(string path){
            // cache the parent folder of a path!
            if( path.length() > 1 ){
                path = rtrim( path, "/" );
            }

            string parent=_dirname(path);
            init( parent );

            if ( isDirCached( parent ) ){
                log_msg( "======> doCachePathParentDir: isDirCached True - %s\n", parent.c_str() );
                return ;
            }
            log_msg("doCachePathParentDir(%s) -> '%s' -> '%s' -> '%s'\n", path.c_str(), remotePath(path), parent.c_str(), BB_DATA->rootdir);
            doCachePathDir( parent );
        }

        bool isDirCached(string path){
            path = fixPath(path);
            if ( path == ".." || path == "."  ){
                return true;
            }
            init(path);

            log_msg("\n\n\tisDirCached: '%s' '%s'\n\n", m_cache[path]["cacheReadDir"].c_str(), path.c_str());
            return exists( m_cache[path]["cacheReadDir"].c_str() );
        }

        bool existsRemote(string path, bool checkLocal=true, int depth=0){
            // if the file or the folder the files is in doenst exist in the remote filesystem, return NotExist
            // this is a HUGE speedup for searchpaths, like PYTHONPATH!!!
            // string __dir=_dirname(path);
            // while( __dir == "/" ){
            // }
            path = fixPath(path);

            if ( path == ".." || path == "." ){
                return true;
            }

            init(path);
            log_msg("\n>>>> existsRemote depth=%d\n", depth);

            if ( exists( m_cache[path]["cacheDirNotExist"].c_str() ) || exists( m_cache[path]["cacheFileNotExist"].c_str() ) ){
                log_msg("\nexistsRemote -> first if - return False - '%s'  '%s'\n",m_cache[path]["cacheDirNotExist"].c_str(), m_cache[path]["cacheFileNotExist"].c_str() );
                return false;
            }

            // we need turn this  on/off for files that only exist locally (fileInSync)
            if(checkLocal){
                // if dir is cached or exists locally, means it exists remotely!
                // if it was deleted remotely, theres another thread wich will delete it locally,
                // so we don't have to check remotely here!!
                log_msg("\nexistsRemote -> second if existLocal(%s)=%d  %d\n", localPath(path), existsLocal(path), exists( localPathLog(path) ));
                if ( existsLocal(path) ){
                    if( isdir(localPath(path)) ){
                        // doCachePathDir( path );
                        log_msg("\nexistsRemote -> second if - is dir %s\n",localPath(path) );
                        return true;
                    } else {
                        bool ret = exists( localPathLog(path) );
                        log_msg("\nexistsRemote -> second if - return true - log exists = %d - %s\n",ret, localPath(path) );
                        return true;
                    }
                }
            }

            // if file doesn't exist locally, but its folder has being cached,
            // it means the file doesn't exist remotely either, so we don't have to
            // query remotely!!
            // if it shows up remotely, the refresh thread should make it appears locally,
            // so the previous if will make it return true!
            log_msg("\nexistsRemote -> third if - %s %d %d\n", path.c_str(), isDirCached( _dirname(path) ),checkLocal );
            if ( isDirCached( _dirname(path) ) ){
                if(checkLocal){
                    // if the cachefile log file exists, means the file exists remotely, But
                    // was deleted locally in the cache only, so in this case, we
                    // should retrieve it again!
                    if( exists( localPathLog(path) ) ){
                        log_msg("\nexistsRemote -> third if log exist - %s\n", localPathLog(path) );
                        struct stat statbuf;
                        lstat( localPathLog(path), &statbuf);
                        remove( localPathLog(path) );
                        // creat( m_cache[path]["cacheFile_log"].c_str(), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
                        // creat( localPath(path), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
                        localFileExist( path );
                        return true;
                    }
                }
                log_msg( "\nexistsRemote -> third if - return False - no cacheFile_log - %s\n", localPathLog(path) );
                return false;
            }

            // END OF LOCAL CHECKS!!
            // Now that we exausted the local checks, there's no option but go to the remote
            // filesystem. But we MUST cache the lookup result so we don't wast time in the future!

            // first, check if the remote folder exists. If not, we already known the files in this folder
            // will not exist localy. So just cache the folder and return not_exist!!!!
            if ( ! exists( _dirname( remotePath(path) ).c_str() ) ){
                log_msg("existsRemote -> quarth if - parent folder of path doesn't exist remotely - return False - %s\n", _dirname( remotePath(path) ).c_str() );
                // set the parent folder as non-existent too!
                localFileNotExist(_dirname(path));
                return false;
            }else{
                if( depth<1 ){
                    doCachePathParentDir( path );

                    // if the folder exists remotely,
                    // lets cache it to avoid future remote query on the same path
                    log_msg("existsRemote -> fifth if  - doCachePath(%s)  \n", path.c_str() );
                    doCachePath( path );
                    bool __isdir = isdir( localPath(path) );
                    if( __isdir ){
                        log_msg("existsRemote -> fifth if  - doCachePathDir(%s)  \n", path.c_str() );
                        doCachePathDir( path );
                    }else{
                    }
                    // call itself again to do the local evaluation again after the caching!!
                    // log_msg("existsRemote -> fifth if  - recurse\n" );
                    // bool ret =  existsRemote( path, checkLocal, depth+1 );
                    bool ret = existsLocal( path );
                    log_msg("existsRemote -> fifth if  - recurse returned %d \n", int(ret) );
                    if( ! ret ){
                        // set the path as non-existent, so next time we don't have to look remotely
                        // if( __isdir ){
                        //     localDirNotExist( path );
                        // }else{
                        // }
                        localFileNotExist( path );
                    }
                    return ret;
                }
            }

            // if path doesn't exist in the remote filesystem, return notExist!
            // also set local cache of the remote state!
            if ( ! exists( remotePath(path) ) ){
                log_msg("existsRemote -> sixth if - don't exist\n" );

                // since we're accessing the remote filesystem, check if the folder of the path exists
                // if( ! existsSymLink( m_cache[path]["cacheFileDir"].c_str() ) ){
                //     localDirNotExist(path);
                // }else{
                // }
                localFileNotExist(path);
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
            if ( path == ".." || path == "."  ){
                return true;
            }


            if ( exists( m_cache[path]["cacheDirNotExist"].c_str() ) || exists( m_cache[path]["cacheFileNotExist"].c_str() ) ){
                log_msg("existsLocal -> first if - return False - '%s' '%s'  '%s'\n",path.c_str(), m_cache[path]["cacheDirNotExist"].c_str(), m_cache[path]["cacheFileNotExist"].c_str() );
                return false;
            }

            // if the the folder of the path doesn't exist locally, return Not Exists already..
            // don't even check for file!
            if ( path!="/" && ! exists( localPathDir(path) ) ){
                log_msg( "existsLocal ->  ! exists( localPathDir(%s) ) = %d - return False", path.c_str(), int(! exists( localPathDir(path) )) );
                return false;
            }

            // if the files exists in the cache filesystem, return EXISTS!
            if ( exists( localPath(path) ) ){
                log_msg( "existsLocal ->  return exists( localPath(%s) ) = %d", path.c_str(), int(exists( localPath(path) )) );
                return exists( localPathLog(path) );
            }

            // else, NOT EXIST!
            return false;
        }

        bool removeFile( string path ){
            this->init( path );
            struct stat statbuf;
            stat(path, &statbuf);
            creat( m_cache[path]["cacheFileNotExist"].c_str(), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
        }

        bool removeDir( string path ){
            removeFile( path );
            // this->init( path );
            // struct stat statbuf;
            // stat(path, &statbuf);
            // creat( m_cache[path]["cacheDirNotExist"].c_str(), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
        }

        bool fileInSync( string path ){
            // get size
            long localSize = 0;
            long remoteSize = 0;
            struct stat cachefile_st;
            log_msg("----------------------\n%s\n---------------------", path.c_str());
            if( existsLocal(path) ){
                localSize = getFileSize( this->localPath(path) );
                log_msg("\nfileInSync exist locally - %s  %s - %ld\n", this->localPath(path), m_cache[path]["cacheFileNotExist"].c_str(), localSize );
                if ( exists( localPathLog(path) ) ){
                    // we do have a log file!!
                    if ( grep( localPathLog(path), "100%" ) ){
                        // log has 100%, so its already synced!
                        // when we create files locally, we already add 100% to the log, so no special
                        // case is needed!!!
                        log_msg("\nlog file exists and has 100%, which means it finished transfering ok!\n" );
                        return true;
                    }else{
                        // log doesn't have 100%, so it need to be synced
                        if( localSize == 0 ){
                            // TODO: check if local file is the same size as remote! (using the size we store in the log file!!)
                            // if size is 0 and no 100%, it needs to be synced!
                            log_msg("\nbut size is 0, so its just an skeleton! (need rsync)\n" );
                            return false;
                        }
                        else{
                            // if size is not zero, then we assume it's synced!!
                            return true;
                        }
                    }
                }
                // if( localSize == 0 ){
                //     // this account for locally created files that don't exist remotely! (COW)
                //     if( ! existsRemote(path, false) ){
                //         log_msg("\nbut size is NOT 0, and it doesn;t exist remotely!\n" );
                //         return true;
                //     }
                // }
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
