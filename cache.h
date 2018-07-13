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
#include <sys/types.h>
#include <sys/syscall.h>

#include <boost/algorithm/string/replace.hpp>

#include "params.h"
#include "log.h"

#include <map>
#include <vector>
#include <string>



// a lstat wrapper to display the caller function in the log!
int _lstat( const char *path, struct stat *buf, const char* caller = __builtin_FUNCTION() )
{
    int ret = lstat( path, buf );
    log_msg("\n==> \t caller(%s) - lstat(%s) = %d\n", caller, path, ret);
    return ret;
}
int __stat( const char *path, struct stat *buf, const char* caller = __builtin_FUNCTION() )
{
    int ret = stat( path, buf );
    log_msg("\n==> \t caller(%s) - stat(%s) = %d\n", caller, path, ret);
    return ret;
}
#define lstat  _lstat
// #define stat  __stat



#include "fileUtils.h"
#include "cache_utils.h"



using namespace std;




static pthread_mutex_t mutex_readlink;
static pthread_mutex_t mutex_cache_path;

static pthread_mutex_t mutex_cache_path_dir;
static pthread_mutex_t mutex_init;
static pthread_mutex_t __mutex_localFileExist;

#define pthread_mutex_wait_unlock( __mutex__ )  \
    pthread_mutex_lock( __mutex__ ); \
    pthread_mutex_unlock( __mutex__ );




#include <exception>
class myexception: public exception
{
  virtual const char* what() const throw()
  {
    log_msg( "!!! Exception!" );
    return "My exception happened";
  }
} except;




static pthread_mutex_t mutex_mkdir_p;
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
        map <string, pthread_mutex_t> m_mutex;

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

    public:
        enum callerFunction { all, no_uid };

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

            string path2 = fixPath( path );

            // check if we already have i
            pthread_mutex_lock(&mutex_init);
            if ( int( m_cache.count( path ) ) > 0 ) {
                // log_msg("=============>init(%s)  return because we already have it initialized! m_cache.size = %d<<<<<<\n",path.c_str(), m_cache.size() );
            }else{
                // log_msg("\n==== init(%s)\n", path.c_str());

                m_cache[path]["p"]                  = boost::replace_all_copy( path2, "/", __sep__ );
                m_cache[path]["cacheFileNotExist"]  = _format( "%s%s.cacheFileNotExist", _cacheControl().c_str(), m_cache[path]["p"].c_str() );
                m_cache[path]["cacheDirNotExist"]   = _format( "%s%s.cacheDirNotExist", _cacheControl().c_str(), boost::replace_all_copy( rtrim( _dirname( path2 ),"/" ), "/" , __sep__ ).c_str() );
                m_cache[path]["cacheLockFile"]      = _format( "%s%s.cacheLockFile",  _cacheControl().c_str(), m_cache[path]["p"].c_str() );
                m_cache[path]["cacheReadDir"]       = _format( "%s%s.cacheReadDir", _cacheControl().c_str(), boost::replace_all_copy( rtrim(path2,"/"), "/" , __sep__ ).c_str() );
                m_cache[path]["cacheFile"]          = _format( "%s%s", _cachePath().c_str(), path2.c_str() );
                m_cache[path]["cacheFileDir"]       = _format( "%s%s", _cachePath().c_str(), _dirname( path2 ).c_str() );
                m_cache[path]["abspath"]            =  boost::replace_all_copy( string( BB_DATA->rootdir ) + "/" + path2, "//" , "/" );
                m_cache[path]["cowFile"]            = _cowPath() + path2;

                m_cache[path]["cacheFile"]          = boost::replace_all_copy( m_cache[path]["cacheFile"], "//" , "/" );
                m_cache[path]["cacheFileDir"]       = boost::replace_all_copy( m_cache[path]["cacheFileDir"], "//" , "/" );
                m_cache[path]["abspath"]            = boost::replace_all_copy( m_cache[path]["abspath"], "//" , "/" );

                m_stat[path]                        = NULL;
                m_mutex[path]                       = PTHREAD_MUTEX_INITIALIZER;

                m_cache[path]["cacheFile_log_suf"]  = "";
                m_cache[path]["cacheFile_log"]      =  _cacheControl();
                m_cache[path]["cacheFile_log"]     += m_cache[path]["p"]+".hradecFS";
                m_cache[path]["cacheFile_log"]     += m_cache[path]["cacheFile_log_suf"];
            }
            pthread_mutex_unlock(&mutex_init);
        }



        void cleanupBeforeStart(){

            pthread_mutex_init( &__mutex_localFileExist , NULL );
            pthread_mutex_init( &mutex_cache_path       , NULL );
            pthread_mutex_init( &mutex_mkdir_p          , NULL );
            pthread_mutex_init( &mutex_readlink         , NULL );
            pthread_mutex_init( &mutex_cache_path_dir   , NULL );
            pthread_mutex_init( &mutex_init             , NULL );

            // remove all leftover lockFiles, if any!
            vector<string> files = glob(_cacheControl() + "*cacheLockFile" );
            for ( unsigned int i = 0; i < files.size(); i++ ) {
                remove( files[i].c_str() );
            }

            // force folders to be updated on first mount
            remove( ( string( _cacheControl() ) + "/.cacheReadDir" ).c_str() );
            vector<string> files2 = glob(_cacheControl() + "*cacheReadDir" );
            for ( unsigned int i = 0; i < files2.size(); i++ ) {
                remove( files2[i].c_str() );
            }
            vector<string> files3 = glob(_cacheControl() + "*__folder__" );
            for ( unsigned int i = 0; i < files3.size(); i++ ) {
                remove( files3[i].c_str() );
            }

            // and links!
            vector<string> files4 = glob(_cacheControl() + "*__link__" );
            for ( unsigned int i = 0; i < files4.size(); i++ ) {
                remove( files4[i].c_str() );
            }

            // and finally logs. We can remove logs to force the remote size of files to be updated
            vector<string> files5 = glob(_cacheControl() + "*.hradecFS" );
            for ( unsigned int i = 0; i < files5.size(); i++ ) {
                remove( files5[i].c_str() );
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
                // log_msg("------------->%s....%s....%d<<<<<<\n",path.c_str(), m_cache[path]["cacheFile_log"].c_str(), m_cache.count(path));
            }

            files4 = glob(_cacheControl() + "*__folder_local__" );
            for ( unsigned int i = 0; i < files4.size(); i++ ) {
                string path = boost::replace_all_copy( getPathFromLogFile( files4[i] ), _cachePath(), "" );
                init( path );
                m_cache[path]["cacheFile_log"] = files4[i];
                // log_msg("------------->%s....%s....%d<<<<<<\n",path.c_str(), m_cache[path]["cacheFile_log"].c_str(), m_cache.count(path));
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
            int uid = -1;
            int retstat=-2;
            log_msg("    >>>  CACHE.stat(%s) uid [%d]\n", path.c_str(), fuse_get_context()->uid);
            // if(m_stat[path]==NULL){
                if( isLocallyCreated( path ) || existsLocal(path) ){
                    log_msg("\n!!! 1\n");
                    retstat = lstat( localPath(path), statbuf );
                    log_msg("\n!!! 1\n" );
                    statbuf->st_size = getPathSizeFromLog( path );
                    log_msg("\n!!! 11\n");
                    struct stat statbufLog;
                    // if local path uid/gid is root, use uid/gid of log file!
                    if( statbuf->st_uid==0 || statbuf->st_gid==0 ){
                        if( lstat( localPathLog(path), &statbufLog ) == 0 ){
                            statbuf->st_uid = statbufLog.st_uid ;
                            statbuf->st_gid = statbufLog.st_gid;
                            // and fix the local file uid/gid, so we don't have to do this again!
                            // chown( localPath(path), statbuf->st_uid, statbuf->st_gid );
                            setStats( path, statbuf );
                        }
                    }
                    uid = statbuf->st_uid;
                }else if( existsRemote( path ) ){
                    retstat = lstat( remotePath(path), statbuf );
                    log_msg("..%s..%d..\n", remotePath(path), statbuf);
                    uid = statbuf->st_uid;
                }else{
                    statbuf=NULL;
                }

                log_msg("\n!!! 2\n");
                if( statbuf != NULL && retstat < 0 ){
                    log_msg("\n!!! 2\n");
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

            log_msg( "    >>>  CACHE.stat(%s) return %d - uid [%d] \n", path.c_str(), retstat, uid );
            return retstat;
        }

        void __checkLogFile( string path ){
            if( getFileSize( localPathLog( path ) ) <5 ){
                log_msg( "\n\t!!! __checkLogFile < 5 - [%s]", path.c_str() );
                remove( localPathLog( path ) );
                localFileExist( path );
            }
        }

        string __parseLogFile( string logfilepath, int column=0, int line=0 ){
            string  ret = "-1";
            char buff[8193];
            int nsplit;
            if( getFileSize( logfilepath ) <5 ){
                return 0;
            }
            // log_msg("\n!!! 4 [%d] [%d] %d %d \n   [%s]\n", pthread_mutex_trylock(&__mutex_localFileExist),
            //        0, EBUSY, EINVAL ,logfilepath.c_str() );

            pthread_mutex_lock(&__mutex_localFileExist);
            int file = open( logfilepath.c_str(), O_RDONLY );
            read( file, buff, 8192 );
            close( file );
            pthread_mutex_unlock(&__mutex_localFileExist);

            string tmp = buff;

            vector<string> lines = split(tmp, '\n');
            if( lines.size() > line ){
                vector<string> tokens = split(lines[line], '*');
                if( tokens.size() > column ){
                    ret = tokens[column].c_str();
                }else{
                    ret = "-1";
                }
            }
            return ret;
        }

        long long getPathSizeFromLog( string path ){
            if( path == "/" ) return 0;
            __checkLogFile( path );
            return  getPathSizeFromLogFile( localPathLog( path ) );
        }
        long long getPathSizeFromLogFile( string logfilepath ){
            return (long long)atoll( __parseLogFile( logfilepath, 1 ).c_str() );
        }
        string getPathFromLog( string path ){
            __checkLogFile( path );
            return  getPathFromLogFile( localPathLog( path ) );
        }
        string getPathFromLogFile( string logfilepath ){
            string ret = __parseLogFile( logfilepath, 0 ).c_str();
            return  ret == "-1" ? "/__NO_PATH_IN_LOG_FILE__" : ret;
        }

        const char * localPathLock(string path){
            init( path );
            return m_cache[path]["cacheLockFile"].c_str();
        }
        const char * localPath(string path){
            init( path );
            return m_cache[path]["cacheFile"].c_str();
        }
        const char * localPathDir(string path){
            init( path );
            return m_cache[path]["cacheFileDir"].c_str();
        }
        const char * remotePath( string path, const char* caller = __builtin_FUNCTION() ){
            init( path );
            log_msg("REMOTE    remotePath(%s) caller(%s)\n", m_cache[path]["abspath"].c_str(), caller );
            return m_cache[path]["abspath"].c_str();
        }

        void refresh(string path){
            init( path );
            m_cache[path]["stat"] = "";
        }

        void localFileNotExist(string path, bool _remove=false){
            const char *ft="cacheFileNotExist";
            if(_remove){
                if( exists( m_cache[path][ft].c_str() ) ){
                    log_msg("   localFileNotExist  remove  %s\n", m_cache[path]["cacheFileNotExist"].c_str());
                    remove( m_cache[path][ft].c_str() );
                }
            }else{
                log_msg("   localFileNotExist   %s\n", m_cache[path]["cacheFileNotExist"].c_str());
                close( creat( m_cache[path][ft].c_str(), 0777 ) );
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

        const char * localPathLog(string path, string suffix="none"){
            init( path );
            if( suffix!="none" ){
                pthread_mutex_lock(&mutex_init);
                if (  m_cache[path]["cacheFile_log_suf"] != suffix ){
                    m_cache[path]["cacheFile_log_suf"] = suffix;
                    m_cache[path]["cacheFile_log"]     =  _cacheControl();
                    m_cache[path]["cacheFile_log"]    += m_cache[path]["p"]+".hradecFS";
                    m_cache[path]["cacheFile_log"]    += m_cache[path]["cacheFile_log_suf"];
                }
                pthread_mutex_unlock(&mutex_init);
            }
            return m_cache[path]["cacheFile_log"].c_str();
        }

        string __logSuffix( string path, bool local=true ){
            string ret = "";
            string localSuffix = local ? "_local__" : "__";

            if ( islnk( localPath( path ) ) ){
                ret = ".__link" + localSuffix;
            }else if ( isdir( localPath( path ) ) ){
                ret = ".__folder" + localSuffix;
            }
            else{
                ret = local ? ".__local__" : "";
            }
            return ret;
        }
        bool isLocallyCreated( string path ){
            return int( m_cache[path]["cacheFile_log"].find( "_local__" ) ) >= 0;
        }
        void setLocallyCreated( string path ){
            localPathLog( path, __logSuffix( path, true ) );
        }
        void setLogFileType( string path ){
            localPathLog( path, __logSuffix( path, false ) );
        }

        void setStats(string path, struct stat *statbuf){ setStats( path.c_str(), statbuf ); }
        void setStats(const char *path, struct stat *statbuf){
            // set all other relevant stats to file/folders!
            // the path is an absolute path, not a index path!
            // this way, we can use this function for localPath() and localPathLog()
            if( ! islnk( path ) ){
                chown( path, statbuf->st_uid, statbuf->st_gid );
                chmod( path, statbuf->st_mode );
                struct utimbuf times;
                times.actime  = statbuf->st_atime;
                times.modtime = statbuf->st_mtime;
                utime( path, &times );
            }
        }

        void localFileExist( string path, callerFunction caller = all ){
            // string linkSuffix   = ".__link__";
            // string fileSuffix   = "";
            // string folderSuffix = ".__folder__";

            // this account for local created files - mknod and write funcions
            // sets the log file to local when creating/modifiying the file,
            // so we known here its local!
            bool localFile =  isLocallyCreated( path );

            log_msg("\n !!! localFileExist: uid [%d]  - isLocallyCreated(%s)=%d  || exists( localPathLog( path ) ) = %d\n", fuse_get_context()->uid, path.c_str(), localFile, exists( localPathLog( path ) ));


            struct stat statbuf;
            memset( &statbuf, 0, sizeof(statbuf) );
            if ( localFile || lstat( remotePath(path), &statbuf ) != 0 ) {
                // if we get here, it means the file doesn't exist remotely, or
                // we knwon its a locally created/modified file!
                struct stat parent_statbuf;
                lstat( localPathDir(path), &parent_statbuf );
                // if no stat, there's no file yet!!
                if ( lstat( localPath(path), &statbuf ) != 0 ) {
                    // so we set a standard stat for the file here
                    statbuf.st_mode  = parent_statbuf.st_mode;
                    statbuf.st_atime = time(NULL);
                    statbuf.st_ctime = time(NULL);
                    if( caller != no_uid && fuse_get_context()->uid != 0 ){
                        statbuf.st_uid   = fuse_get_context()->uid;
                        statbuf.st_gid   = fuse_get_context()->gid;
                    }
                }
                // linkSuffix   = ".__link_local__";
                // folderSuffix = ".__folder_local__";
                // fileSuffix   = ".__local__";
            };

            if( localFile ){
                log_msg("\n !!! \tlocalFileExist: if(localFile) = true\n");
                if( caller != no_uid && fuse_get_context()->uid != 0 ){
                    statbuf.st_uid   = fuse_get_context()->uid;
                    statbuf.st_gid   = fuse_get_context()->gid;
                }
                statbuf.st_mtime = time(NULL);
                statbuf.st_atime = time(NULL);

                // if the file uid/gid is zero, force the uid/gid of the caller user
                // hopefully this will fix permissions if something goes wrong
                if( statbuf.st_uid == 0 )
                    statbuf.st_uid = fuse_get_context()->uid;
                if( statbuf.st_gid == 0 )
                    statbuf.st_gid = fuse_get_context()->gid;

            // if the file is not locally created/modified, and the localPathLog doesn't exist, we need
            // to get the stat from the remote!
            }else if( ! exists( localPathLog( path ) ) ) {
                log_msg( "\nREMOTE !!! localFileExist: lstat( %s )\n", path.c_str() );
                lstat( remotePath(path), &statbuf );
            }

            // from this point forward, statbuf is initialized and will be used to create the cache files.

            // create the skeleton file`
            if ( ! exists( localPath( path ) ) ) {
                close( creat( localPath( path ), statbuf.st_mode & ( S_IRWXU | S_IRWXG | S_IRWXO ) ) );
            }

            // set remote file stats to file
            setStats( localPath( path ), &statbuf );

            // fix log file (file, folder and link and local)
            if( localFile ){
                setLocallyCreated( path );
                lstat( localPath(path), &statbuf );
            }else{
                setLogFileType( path );
            }
            // if ( islnk( localPath( path ) ) ){
            //     if ( int( m_cache[path]["cacheFile_log"].find(linkSuffix) ) < 0 )
            //         m_cache[path]["cacheFile_log"] += linkSuffix;
            // }else if ( isdir( localPath( path ) ) ){
            //     if ( int( m_cache[path]["cacheFile_log"].find(folderSuffix) ) < 0 )
            //         m_cache[path]["cacheFile_log"] = m_cache[path]["cacheFile_log"] + folderSuffix;
            // }else if ( fileSuffix != "" ) {
            //     if ( int( m_cache[path]["cacheFile_log"].find(fileSuffix) ) < 0 )
            //         m_cache[path]["cacheFile_log"] = m_cache[path]["cacheFile_log"] + fileSuffix;
            // }

            if( ! exists( localPathLog( path ) ) || localFile ){
                // create a log file for every file/folder/link
                // we use the log file to known what's in the remote side and what's not!
                string tmp = _format( "%s*%lld\n", localPath( path ) , (long long) statbuf.st_size );

                pthread_mutex_lock(&__mutex_localFileExist);
                remove( localPathLog( path ) );
                int file = creat( localPathLog( path ), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
                write( file, tmp.c_str(),  tmp.length() );
                close( file );
                pthread_mutex_unlock(&__mutex_localFileExist);

                // set remote file stats to log file as well
                // but time should be current!!
                statbuf.st_mtime = time(NULL);
                statbuf.st_atime = time(NULL);
                setStats( localPathLog( path ), &statbuf);
                log_msg( "\n !!! localFileExist: create skeleton %s --> %s \n",  localPathLog( path ), localPath( path ) );
            }
            localFileNotExistRemove( path );
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

                log_msg( "\n\nmkdir: %s  \n", localPathLog(path) );
                // localFileNotExistRemove(path);
                // if its a folder cache, pre-cache it!
                // if(__depth<=1){
                //     doCachePathDir(path, __depth+1);
                // }
            }else{
                localFileNotExist( path );
                return;
            }
            localFileExist(path);
        }

        void doCachePathDir(string path, int __depth=0){
            // do a initial cache of the entire folder of a path, so we can
            // collect all the data we can using the same time of a single path.
            init(path);

            // if already cached, don't cache again!
            path = fixPath(path);
            if ( isDirCached( path ) || isLocallyCreated( path ) ){
                log_msg( "======> doCachePathDir: isDirCached True - %s / locallyCreated = \n", path.c_str(), isLocallyCreated( path ) );
                return ;
            }
            log_msg( "\nREMOTE ====> doCachePathDir: %s -> %s\n", path.c_str(), remotePath(path) );

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
            if( getdir( fixPath( remotePath(path) ) + "/", files ) )
                log_msg( "!!!! exception remotepath getidr(%s)\n", (fixPath( remotePath(path) ) + "/").c_str() );
                //throw except;

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

            string completePath="";
            string parent = fixPath( _dirname(path) );

            vector<string> p = split(parent, '/');
            for ( unsigned int i = 0; i < p.size(); i++ ) {
                completePath += "/"+ p[i];
                init( completePath );
                if ( ! isDirCached( completePath ) ){
                    log_msg("=== > doCachePathParentDir(%s) -> '%s' -> '%s' -> '%s'\n", path.c_str(), remotePath(path), parent.c_str(), BB_DATA->rootdir);
                    doCachePathDir( completePath );
                }
            }

        }

        void readDirCached(string path){
            init( path );
            log_msg("   readDirCached   cacheReadDir: %s\n", m_cache[path]["cacheReadDir"].c_str());

            pthread_mutex_lock(&mutex_cache_path_dir);
            close( creat( m_cache[path]["cacheReadDir"].c_str(), 0777 ) );
            pthread_mutex_unlock(&mutex_cache_path_dir);

            setLogFileType( path );
            // if ( m_cache[path]["cacheFile_log"].find(".__folder__") == string::npos )
            //     m_cache[path]["cacheFile_log"] += ".__folder__";
            localFileExist(path);
        }

        bool isDirCached(string path){
            path = fixPath(path);
            if ( path == ".." || path == "."  ){
                return true;
            }
            init(path);

            log_msg("\n\n\tisDirCached: '%s' '%s'\n\n", m_cache[path]["cacheReadDir"].c_str(), path.c_str());
            pthread_mutex_lock(&mutex_cache_path_dir);
            bool ret=exists( m_cache[path]["cacheReadDir"].c_str() );
            pthread_mutex_unlock(&mutex_cache_path_dir);

            return ret;
        }

        bool existsRemote(string path, int depth=0){
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

            // if dir is cached or exists locally, means it exists remotely!
            // if it was deleted remotely, theres another thread wich will delete it locally,
            // so we don't have to check remotely here!!
            log_msg("\nexistsRemote -> second if existLocal(%s)\n", localPath(path));
            if ( existsLocal(path) ){
                if( isdir(localPath(path)) ){
                    // doCachePathDir( path );
                    log_msg("\nexistsRemote -> second if - is dir %s\n",localPath(path) );
                    return true;
                } else {
                    bool ret = exists( localPathLog(path) );
                    log_msg("\nexistsRemote -> second if - return true - log exists = %d - %s\n",ret, localPathLog(path) );
                    return true;
                }
            }

            // if file doesn't exist locally, but its folder has being cached,
            // it means the file doesn't exist remotely either, so we don't have to
            // query remotely!!
            // if it shows up remotely, the refresh thread should make it appears locally,
            // so the previous if will make it return true!
            log_msg("\nexistsRemote -> third if - %s %d\n", path.c_str(), isDirCached( _dirname(path) ) );
            if ( isDirCached( _dirname(path) ) ){
                // if the cachefile log file exists, means the file exists remotely, But
                // was deleted locally in the cache only, so in this case, we
                // should retrieve it again!
                if( exists( localPathLog(path) ) ){
                    log_msg("\nexistsRemote -> third if log exist - %s\n", localPathLog(path) );
                    struct stat statbuf;
                    lstat( localPathLog(path), &statbuf);
                    remove( localPathLog(path) );
                    localFileExist( path );
                    return true;
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
                doCachePathParentDir( path );
                log_msg("REMOTE existsRemote -> quarth if - parent folder of path doesn't exist remotely - return False - %s\n", _dirname( remotePath(path) ).c_str() );
                // set the parent folder as non-existent too!
                localFileNotExist(_dirname(path));
                return false;
            }else{
                if( depth<1 ){
                    doCachePathParentDir( path );

                    // if the folder exists remotely,
                    // lets cache it to avoid future remote query on the same path
                    log_msg("REMOTE existsRemote -> fifth if  - doCachePath(%s)  \n", path.c_str() );
                    doCachePath( path );
                    bool __isdir = isdir( localPath(path) );
                    if( __isdir ){
                        log_msg("REMOTE existsRemote -> fifth if  - doCachePathDir(%s)  \n", path.c_str() );
                        doCachePathDir( path );
                    }else{
                    }
                    // call itself again to do the local evaluation again after the caching!!
                    // log_msg("existsRemote -> fifth if  - recurse\n" );
                    bool ret = existsLocal( path );
                    log_msg("REMOTE existsRemote -> fifth if  - recurse returned %d \n", int(ret) );
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
                log_msg("REMOTE existsRemote -> sixth if - don't exist\n" );

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
        bool existsLocal( string path, const char* caller = __builtin_FUNCTION() ){

            // if the file or the folder the files is in doenst exist in the remote filesystem, return NotExist
            // this is a HUGE speedup for searchpaths, like PYTHONPATH!!!
            // string __dir=_dirname(path);
            // while( __dir == "/" ){
            // }
            if ( path == ".." || path == "."  ){
                return true;
            }


            if ( exists( m_cache[path]["cacheDirNotExist"].c_str() ) || exists( m_cache[path]["cacheFileNotExist"].c_str() ) ){
                log_msg("%s | existsLocal -> first if - return False - '%s' '%s'  '%s'\n",caller, path.c_str(), m_cache[path]["cacheDirNotExist"].c_str(), m_cache[path]["cacheFileNotExist"].c_str() );
                return false;
            }

            // if the the folder of the path doesn't exist locally, return Not Exists already..
            // don't even check for file!
            if ( path!="/" && ! exists( localPathDir(path) ) ){
                log_msg( "%s | existsLocal ->  ! exists( localPathDir(%s) ) = %d - return False", caller, path.c_str(), int(! exists( localPathDir(path) )) );
                return false;
            }

            // if the files exists in the cache filesystem, return EXISTS!
            if ( exists( localPath(path) ) ){
                log_msg( "%s | existsLocal ->  return exists( localPath(%s) ) = %d", caller, path.c_str(), int(exists( localPath(path) )) );
                return bool( exists( localPathLog(path) ) );
            }

            // else, NOT EXIST!
            return false;
        }

        bool removeFile( string path ){
            this->init( path );
            struct stat statbuf;
            // stat(path, &statbuf);
            close( creat( m_cache[path]["cacheFileNotExist"].c_str(), S_IRWXU | S_IRWXG | S_IRWXO ) );
        }

        bool removeDir( string path ){
            removeFile( path );
            // this->init( path );
            // struct stat statbuf;
            // stat(path, &statbuf);
            // creat( m_cache[path]["cacheDirNotExist"].c_str(), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
        }

        bool fileInSync( string path ){
            // returns if a files is up to date with the remote file system

            long long localSize = 0;
            long remoteSize = 0;
            struct stat cachefile_st;
            log_msg("----------------------\n%s\n---------------------", path.c_str());

            // if the file exists locally (and it should, since all remote files are created locally
            // when a folder is cached, toguether with it's control files (log, etc) ).
            if( existsLocal(path) ){
                // if the log file has __local__ suffix, it means it's
                // a file that has being created/modified locally.
                // in this case, we just return true!
                if ( isLocallyCreated(path) ){
                    log_msg("\nfileInSync is locally created/modified, so it does exist indeed! [%s]", localPathLog(path));
                    return true;
                }

                // if its no a local created/modified file, lets check if it's up2date!
                localSize = getFileSize( localPath(path) );
                log_msg("\nfileInSync exist locally - [%s]  [%s] [%s] - [%ld]\n", localPath(path),
                    m_cache[path]["cacheFileNotExist"].c_str(), localPathLog(path), localSize );

                // if the log control file exists (and it should since we created it when a folder is cached),
                // compare the size of the local file with the remote files size (pre-stored in the log!)
                if ( exists( localPathLog(path) ) ){
                    // check local size against remote size (stored in the log file)
                    if( localSize != getPathSizeFromLog( path ) ){
                        log_msg("\nbut size is different than remote, so its just an skeleton! (need rsync)\n" );
                        // size doesn't match, so not in sync!
                        return false;
                    }
                    // if the sizes are the same, it is in sync indeed!
                    return true;
                }
            }

            // this point on should only be executed in cases where we're trying to check
            // if a file is in sync, but that files DOESN'T exist locally.

            // This SHOULD NEVER HAPPEN, since we make sure to cache the parent folder when a file is needed.
            // so if we EVER get to this point, SOMETHING IS WRONG SOMEWHERE ELSE!

            // not sure how what's the best way to handle this is, but for now we access the remote filesystem
            // to get the remote file size and compare with the local one.
            // and since we're acessing the remote filesystem anyway, we FIX the control log file now,
            // so the next time this file is checked, the log has the remote file size correctly and we
            // don't have to do this again!!

            // this could happen if the cache/control files get damaged somehow, or if there's a BUG
            // somewhere else in this class code!

            log_msg("\nfileInSync check size of local against remote file (access the remote filesystem for that - SLOW)."
                "if this code is being executed, it means the control log file doesn't have the remote file size."
                "This SHOULD NOT HAPPEN!"
                "But since this code is being executed, it happened, so we can handle it here by accessing the"
                "remote file system to retrieve the remote file size."
                "Since we're doing that anyway, we can re-create the control log file and fix this problem."
            "\n" );

            // get size
            struct stat fpath_st;
            stat((const char *)this->remotePath(path), &fpath_st);
            remoteSize = getFileSize( remotePath(path) );

            log_msg( "\n\tfileInSync  local path: %s size: %lld \n",  localPath(path), localSize );
            log_msg( "\nREMOTE \tfileInSync remote path: %s size: %lld \n", remotePath(path), remoteSize );

            // check if the 2 sizes are the same or not!
            bool ret = localSize == remoteSize;
            log_msg( "\n\tfileInSync result: %d \n", ret );

            // and since we slowed down anyway, lets fix the control log file
            localFileExist( path );

            return ret;
        }

        bool openFiles( string path, struct fuse_file_info *fi ){

        };
};

// TODO: central class to manage preference files at root of the cache file system!
// class __cache_setup_files {
//     __cache CACHE;
//     public:
//         __cache_setup_files( __cache &cache ) : CACHE(cache) {}
//
//         int read_local_files( string path, char *buf ){
//             if( path  == "/.hradecFS_local_files" ){
//                 string p;
//                 string files = "";
//                 sprintf( buf, "" );
//                 vector<string> zfiles = glob(CACHE._cacheControl() + "*.__local__" );
//                 for ( unsigned int i = 0; i < zfiles.size(); i++ ) {
//                     if( int( zfiles[i].find(".hradecFS_") ) < 0 ){
//                         p = CACHE.getPathFromLogFile( zfiles[i] );
//                         if( isfile( p.c_str() ) )
//                             sprintf( buf, string( p + "\n" + buf ).c_str() );
//                     }
//                 }
//             }
//         }
// }
