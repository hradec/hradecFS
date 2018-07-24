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
#include <boost/filesystem.hpp>
#include <time.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/format.hpp>

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
// #define lstat  _lstat
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
        map <string, pthread_mutex_t> m_mutex_cache;
        map <string, pthread_mutex_t> m_mutex_cacheDir;
        map <string, pthread_mutex_t> m_mutex_logfile;

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

        string __fixFileName( string fileName ){
            if( fileName.length() > 210 )
                fileName = fileName.substr( fileName.size() - 210 );
            return fileName;
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
                // m_mutex[path]                       = PTHREAD_MUTEX_INITIALIZER;
                pthread_mutex_init( &m_mutex[path], NULL );
                pthread_mutex_init( &m_mutex_cache[path], NULL );
                pthread_mutex_init( &m_mutex_cacheDir[path], NULL );


                m_cache[path]["cacheFile_log_suf"]  = "";
                m_cache[path]["cacheFile_log"]      =  _cacheControl();
                m_cache[path]["cacheFile_log"]     += m_cache[path]["p"]+".hradecFS";
                m_cache[path]["cacheFile_log"]     += m_cache[path]["cacheFile_log_suf"];

                // we have to garantee the log file is smaller than the maximum of 255 file name lenght.
                m_cache[path]["cacheFile_log"]      = __fixFileName( m_cache[path]["cacheFile_log"] );
                pthread_mutex_init( &m_mutex_logfile[m_cache[path]["cacheFile_log"]], NULL );
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

            // remove control files!
            // boost::filesystem::remove_all( _cacheControl() );
            // mkdir_p( _cacheControl().c_str() );

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
            vector<string> files6 = glob(_cacheControl() + "*cacheFileNotExist" );
            for ( unsigned int i = 0; i < files6.size(); i++ ) {
                remove( files6[i].c_str() );
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

        int _stat(string path){
            struct stat statbuf;
            _stat( path, &statbuf );
        }

        int _stat(string path, struct stat *statbuf, const char* caller = __builtin_FUNCTION()){
            // get path STAT and cache it.
            int uid = -1;
            int retstat=-2;
            log_msg("\n    >>>  CACHE.stat(%s) uid [%d]  caller: %s\n", path.c_str(), fuse_get_context()->uid, caller);
            // if(m_stat[path]==NULL){

                // if we deleted it locally, it doesn't exist!
                if ( exists( m_cache[path]["cacheDirNotExist"].c_str() ) || exists( m_cache[path]["cacheFileNotExist"].c_str() ) ){
                    retstat=-2;
                }else
                if( isLocallyCreated( path ) || existsLocal( path ) || existsRemote( path ) ){
                    log_msg("\n!!! 1\n");
                    retstat = lstat( localPath(path), statbuf );
                    log_msg("\n!!! 1\n" );
                    // getPathSizeFromLog will return -1 if something goes wrong!
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
                }

                // if the parent folder doesn't exist, we need to check on the remote side!
                // if the parent exists, means the file doesn't exist remotely either!
                // }else if( existsRemote(path) ){
                //     retstat = lstat( localPath(path), statbuf );
                //     log_msg("!!!  2 ..%s..%d..\n", remotePath(path), statbuf);
                //     uid = statbuf->st_uid;

                if( retstat < 0  ||  statbuf->st_size<0 ){
                    log_msg("\n!!! 3\n");
                    memset( statbuf, 0, sizeof(statbuf) );
                    retstat = -2;
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

            log_msg( "\n    >>>  CACHE.stat(%s) return %d - uid [%d] \n", path.c_str(), retstat, uid );
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
                return ret;
            }
            // log_msg("\n!!! 4 [%d] [%d] %d %d \n   [%s]\n", pthread_mutex_trylock(&__mutex_localFileExist),
            //        0, EBUSY, EINVAL ,logfilepath.c_str() );

            // char * mutex_in_case_logfilepath_changes = logfilepath.c_str();
            // pthread_mutex_lock( &m_mutex_logfile[ mutex_in_case_logfilepath_changes ] );
            int file = open( logfilepath.c_str(), O_RDONLY );
            read( file, buff, 8192 );
            close( file );
            // pthread_mutex_unlock( &m_mutex_logfile[ mutex_in_case_logfilepath_changes ] );

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

        long long getPathSizeFromLogFile( string logfilepath ){
            long long ret = (long long)atoll( __parseLogFile( logfilepath, 1 ).c_str() );
            return ret;
        }
        long long getPathSizeFromLog( string path ){
            if( path == "/" ) return 0;
            __checkLogFile( path );
            mutex_lock_cache(path);
            long long ret =  getPathSizeFromLogFile( localPathLog( path ) );
            mutex_unlock_cache(path);
            return ret;
        }

        string getPathFromLogFile( string logfilepath ){
            string ret = __parseLogFile( logfilepath, 0 ).c_str();
            return  ret == "-1" ? "/__NO_PATH_IN_LOG_FILE__" : ret;
        }
        string getPathFromLog( string path ){
            __checkLogFile( path );
            mutex_lock_cache(path);
            string ret = getPathFromLogFile( localPathLog( path ) );
            mutex_unlock_cache(path);
            return ret;
        }

        const char * localPathLock(string path){
            init( path );
            return m_cache[path]["cacheLockFile"].c_str();
        }
        const char * localPath(string path){
            return m_cache[path]["cacheFile"].c_str();
        }
        pthread_mutex_t * mutex(string path){
            return &m_mutex[path];
        }
        void mutex_lock( string path, const char* caller = __builtin_FUNCTION() ){
            log_msg6("\n====>mtx_lock  uid: [%4d]  path: %s  mutex: 0x%p  caller: %s\n", fuse_get_context()->uid, path.c_str(), mutex(path), caller);
            pthread_mutex_lock(&m_mutex[path]);
        }
        void mutex_unlock( string path, const char* caller = __builtin_FUNCTION() ){
            log_msg6("\n====>mtx_unlok uid: [%4d]  path: %s  mutex: 0x%p  caller: %s\n", fuse_get_context()->uid, path.c_str(), mutex(path), caller);
            pthread_mutex_unlock(&m_mutex[path]);
        }

        void mutex_lock_cache( string path, const char* caller = __builtin_FUNCTION() ){
            log_msg6("\n====>mtx_lock_cache  uid: [%4d]  path: %s  mutex: 0x%p  caller: %s\n", fuse_get_context()->uid, path.c_str(), &m_mutex_cache[path], caller);
            pthread_mutex_lock(&m_mutex_cache[path]);
        }
        void mutex_unlock_cache( string path, const char* caller = __builtin_FUNCTION() ){
            log_msg6("\n====>mtx_unlok_cache uid: [%4d]  path: %s  mutex: 0x%p  caller: %s\n", fuse_get_context()->uid, path.c_str(), &m_mutex_cache[path], caller);
            pthread_mutex_unlock(&m_mutex_cache[path]);
        }

        void mutex_lock_cacheDir( string path, const char* caller = __builtin_FUNCTION() ){
            log_msg6("\n====>mtx_lock_cacheDir  uid: [%4d]  path: %s  mutex: 0x%p   caller: %s\n", fuse_get_context()->uid, path.c_str(), &m_mutex_cacheDir[path], caller);
            pthread_mutex_lock(&m_mutex_cacheDir[path]);
        }
        void mutex_unlock_cacheDir( string path, const char* caller = __builtin_FUNCTION() ){
            log_msg6("\n====>mtx_unlok_cacheDir uid: [%4d]  path: %s  mutex: 0x%p   caller: %s\n", fuse_get_context()->uid, path.c_str(), &m_mutex_cacheDir[path], caller);
            pthread_mutex_unlock(&m_mutex_cacheDir[path]);
        }

        const char * localPathDir(string path){
            init( path );
            return m_cache[path]["cacheFileDir"].c_str();
        }
        const char * remotePath( string path, const char* caller = __builtin_FUNCTION() ){
            init( path );
            // log_msg("REMOTE    remotePath(%s) caller(%s)\n", m_cache[path]["abspath"].c_str(), caller );
            return m_cache[path]["abspath"].c_str();
        }

        void refresh(string path){
            init( path );
            m_cache[path]["stat"] = "";
        }

        void localFileNotExist(string path, bool _remove=false){
            if(_remove){
                if( exists( m_cache[path]["cacheFileNotExist"].c_str() ) ){
                    log_msg("   localFileNotExist  remove  %s\n", m_cache[path]["cacheFileNotExist"].c_str());
                    remove( m_cache[path]["cacheFileNotExist"].c_str() );
                }
            }else{
                log_msg("   localFileNotExist   %s = %s\n", path.c_str(), m_cache[path]["cacheFileNotExist"].c_str());
                close( creat( m_cache[path]["cacheFileNotExist"].c_str(), 0777 ) );

                if( exists( localPath( path ) ) )
                    boost::filesystem::remove_all( localPath( path ) );
                if( exists( localPathLog( path ) ) )
                    boost::filesystem::remove_all( localPathLog( path ) );
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
                pthread_mutex_lock( &mutex_init );
                // char * old_mutex = m_cache[path]["cacheFile_log"].c_str();
                // pthread_mutex_lock( &m_mutex_logfile[ old_mutex ] );
                if (  m_cache[path]["cacheFile_log_suf"] != suffix ){
                    m_cache[path]["cacheFile_log_suf"] = suffix;
                    m_cache[path]["cacheFile_log"]     =  _cacheControl();
                    m_cache[path]["cacheFile_log"]    += m_cache[path]["p"]+".hradecFS";
                    m_cache[path]["cacheFile_log"]    += m_cache[path]["cacheFile_log_suf"];
                }
                // pthread_mutex_init( &m_mutex_logfile[m_cache[path]["cacheFile_log"]], NULL );
                // pthread_mutex_unlock( &m_mutex_logfile[ old_mutex ] );
                pthread_mutex_unlock( &mutex_init );
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
            bool ret = int( m_cache[path]["cacheFile_log"].find( "_local__" ) ) >= 0;
            // we have to check if the logFile exists, or else we need to reset the
            // the log file back to NOT being local!
            if ( ret && ! exists( m_cache[path]["cacheFile_log"].c_str() ) ) {
                ret = false;
                pthread_mutex_lock( &mutex_init );
                m_cache[path]["cacheFile_log_suf"]  = "";
                m_cache[path]["cacheFile_log"]      =  _cacheControl();
                m_cache[path]["cacheFile_log"]     += m_cache[path]["p"]+".hradecFS";
                m_cache[path]["cacheFile_log"]     += m_cache[path]["cacheFile_log_suf"];

                // we have to garantee the log file is smaller than the maximum of 255 file name lenght.
                pthread_mutex_init( &m_mutex_logfile[m_cache[path]["cacheFile_log"]], NULL );
                pthread_mutex_unlock( &mutex_init );
            }
            return ret;
        }
        void setLocallyCreated( string path ){
            close( creat(
                localPathLog( path, __logSuffix( path, true ) )
            , 0777 ) );
        }
        void setLogFileType( string path, bool locallyCreated=false ){
            localPathLog( path, __logSuffix( path, locallyCreated ) );
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
            init(path);
            bool localFile =  isLocallyCreated( path );

            log_msg("\n !!! localFileExist: uid [%d]  - isLocallyCreated(%s)=%d  || exists( localPathLog( path ) ) = %d\n", fuse_get_context()->uid, path.c_str(), localFile, exists( localPathLog( path ) ));

            time_t timer;

            bool shouldSetStat=false;
            struct stat statbuf;
            memset( &statbuf, 0, sizeof(statbuf) );

            //if ( localFile || lstat( remotePath(path), &statbuf ) != 0 ) {
            if ( localFile ) {
                // if we get here, it means the file is created/modified locally!
                log_msg("\n !!! \tlocalFileExist: if(localFile) = true\n");

                // we get the stat of the local file and also check if it exists!!
                if ( lstat( localPath(path), &statbuf ) == 0 ) {

                    // fix current mtime and atime!
                    if( statbuf.st_mtime == 0 )
                        statbuf.st_mtime = time(NULL);
                    if( statbuf.st_atime == 0 )
                        statbuf.st_atime = time(NULL);

                    // if its locally created and the UID is root, set
                    // the current user as UID/GID!
                    // hopefully this will fix permissions if something goes wrong
                    if(  fuse_get_context()->uid != 0 && statbuf.st_uid != fuse_get_context()->uid && caller != no_uid ){
                        // fix UID if wrong
                        log_msg5("\n====>fixUID    uid: [%4d]  statUID: [%4d] path: %s\n", fuse_get_context()->uid, statbuf.st_uid, path.c_str());
                        if( statbuf.st_uid == 0 )
                            statbuf.st_uid = fuse_get_context()->uid;
                        if( statbuf.st_gid == 0 )
                            statbuf.st_gid = fuse_get_context()->gid;
                    }

                    if( statbuf.st_mode == 0)
                        statbuf.st_mode = ( S_IRWXU | S_IRWXG | S_IRWXO );

                    shouldSetStat=true;
                }

            }else if( ! exists( localPathLog( path ) ) ) {
                // if the file is not locally created/modified, and the localPathLog
                // doesn't exist, we need to get the stat from the remote!
                log_msg( "\nREMOTE !!! localFileExist: !exists(localPathLog()) lstat( %s )\n", path.c_str() );
                lstat( remotePath(path), &statbuf );
                shouldSetStat=true;

            }else{
                // so, its not Local and the local pathlog exists!
                // in this case, we grab the stat from the actual local cache file!
                lstat( localPathLog(path), &statbuf );
                // also instruct the next code to NOT re-update the stats on the file,
                // since they don't change!
                shouldSetStat=false;
            }

            // set the correct log file name, depending on the type of entry (dir/file/link, local or not)
            setLogFileType( path, localFile );

            // from this point forward, statbuf is initialized and will be used to set the cache  files.

            //create the skeleton file (the cache file, no the log file), if
            // it doesn't exist yet!
            if ( ! exists( localPath( path ) ) )
                close( creat( localPath( path ), statbuf.st_mode & ( S_IRWXU | S_IRWXG | S_IRWXO ) ) );

            // set the stats of the cache file (not the log file)
            if(shouldSetStat){
                setStats( localPath( path ), &statbuf );
            }

            if( ! exists( localPathLog( path ) ) || localFile ){
                // create a log file for every file/folder/link
                // we use the log file to known what's in the remote side and what's not!
                time(&timer);
                string tmp = _format( "%s*%lld*%lld*%lld\n", localPath( path ) , (long long) statbuf.st_size,
                                                            (long long) statbuf.st_ino, (long long)timer );

                // const char * old_mutex = localPathLog( path );
                // pthread_mutex_lock( &m_mutex_logfile[ old_mutex ] );
                mutex_lock_cache(path);
                remove( localPathLog( path ) );
                int file = creat( localPathLog( path ), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
                write( file, tmp.c_str(),  tmp.length() );
                close( file );
                mutex_unlock_cache(path);
                // pthread_mutex_unlock( &m_mutex_logfile[ old_mutex ] );

                // set remote file stats to log file as well
                // but time should be current!!
                statbuf.st_mtime = time(NULL);
                statbuf.st_atime = time(NULL);
                setStats( localPathLog( path ), &statbuf);
                setStats( localPath( path ), &statbuf );
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
            path = fixPath(path);

            init(path);
            char buf[8192];
            unsigned long len;

            char * mutex = path.c_str();
            // mutex_lock_cache(mutex);


            // we dont need to check the return of lstat, since we KNOWN at this point that
            // the path exists!
            log_msg("1.doCachePath %s", remotePath(path) );
            lstat(remotePath(path), statbuf);

            if ( islnk( remotePath(path) ) ){
                if ( (len = readlink(remotePath(path), buf, sizeof(buf)-1)) != -1 ){
                    // cleanup if theres another entry with the same name.
                    if( exists( localPath( path ) ) && isdir( localPath(path) ) ){
                        boost::filesystem::remove_all( localPath( path ) );
                    }else{
                        boost::filesystem::remove( localPath( path ) );
                    }
                    buf[len] = '\0';
                    string linkPath = buf;

                    // if the link is absolute, we have to fix it so it works in the
                    // cache filesystem correctly.
                    if( buf[0] == '/' ){
                        // for now, we just simply prefix the mount dir to the path
                        linkPath = fixPath( string(BB_DATA->mountdir)+"/"+buf );
                    }

                    symlink( linkPath.c_str(), localPath(path) );
                    log_msg( "\nlink path %s = remotePath(%s) %s \n", buf, remotePath(path), localPath(path) );
                }
            }else
            if( isfile( remotePath(path) ) ){
                // cleanup if theres another entry with the same name.
                if( exists( localPath( path ) ) && ! isfile( localPath(path) ) ){
                    boost::filesystem::remove_all( localPath( path ) );
                }
            }else if( isdir( remotePath(path) ) ){
                // cleanup if theres another entry with the same name.
                if( exists( localPath( path ) ) && ! isdir( localPath(path) ) ){
                    remove( localPath( path ) );
                }
                // create the folder locally!
                int ret = mkdir( localPath(path), statbuf->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
                log_msg( "\n\nmkdir: %s  %d \n", localPath(path), ret );

            }else{
                localFileNotExist( path );
                // mutex_unlock_cache(mutex);
                return;
            }
            localFileExist(path);
            // mutex_unlock_cache(mutex);
        }

        void doCachePathDir(string path, int __depth=0, const char* caller = __builtin_FUNCTION()){
            // do a initial cache of the entire folder of a path, so we can
            // collect all the data we can using the same time of a single path.
            init(path);


            // if already cached, don't cache again!
            path = fixPath(path);
            if ( isDirCached( path ) || isLocallyCreated( path ) ){
                log_msg( "\n======> doCachePathDir: isDirCached True - %s / locallyCreated = \n", path.c_str(), isLocallyCreated( path ) );
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
                log_msg( "!!!! exception remotepath getdir(%s)\n", (fixPath( remotePath(path) ) + "/").c_str() );
                //throw except;

            for ( unsigned int i = 0; i < files.size(); i++ ) {
                if( files[i].find(".fuse_hidden") != std::string::npos ){
                    continue;
                }else
                if( files[i].find(".nfs00") != std::string::npos ){
                    continue;
                }
                struct stat statbuf2;
                string glob_path = boost::replace_all_copy( files[i], BB_DATA->rootdir, "" );
                log_msg("---getdir-------->%s - caller %s\n", glob_path.c_str(),caller );
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

        void doCachePathParentDir(string path, const char* caller = __builtin_FUNCTION()){
            // cache the parent folder of a path!
            if( path.length() > 1 ){
                path = rtrim( path, "/" );
            }

            string completePath="";
            string parent = fixPath( _dirname(path) );

            vector<string> p = split(parent, '/');
            for ( unsigned int i = 0; i < p.size(); i++ ) {
                completePath += "/"+ p[i];
                completePath = fixPath(completePath);
                init( completePath );
                if ( ! isDirCached( completePath ) &&  ! isLocallyCreated( completePath ) ){
                    log_msg("=== > doCachePathParentDir(%s) -> '%s' -> '%s' -> '%s' - caller %s\n", path.c_str(), remotePath(path), parent.c_str(), BB_DATA->rootdir, caller);
                    doCachePathDir( completePath );
                }
            }

        }

        void readDirCached(string path){
            init( path );
            log_msg("   readDirCached   cacheReadDir: %s\n", m_cache[path]["cacheReadDir"].c_str());

            // pthread_mutex_lock(&mutex_cache_path_dir);
            mutex_lock_cacheDir(path);
            close( creat( m_cache[path]["cacheReadDir"].c_str(), 0777 ) );
            mutex_unlock_cacheDir(path);
            // pthread_mutex_unlock(&mutex_cache_path_dir);

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
            // pthread_mutex_lock(&mutex_cache_path_dir);
            // mutex_lock_cacheDir(path);
            bool ret=exists( m_cache[path]["cacheReadDir"].c_str() );
            // mutex_unlock_cacheDir(path);
            // pthread_mutex_unlock(&mutex_cache_path_dir);

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

            // if the entry has been marked as inexistent, we already return false here, since it doesn't exist!
            if ( exists( m_cache[path]["cacheDirNotExist"].c_str() ) || exists( m_cache[path]["cacheFileNotExist"].c_str() ) ){
                log_msg("\nexistsRemote -> first if - return False - '%s'  '%s'\n",m_cache[path]["cacheDirNotExist"].c_str(), m_cache[path]["cacheFileNotExist"].c_str() );
                return false;
            }

            // the next 2 if's are responsable for fixing problems in the cache, and consenquently they
            // can be used to update entrys in the cache.
            // to update a folder - remove the folder from the /filesystem/ folder
            // to update a file   - remove the file from the /filesystem/ folder
            //
            // NEVER mess with the files in the /control/ folder! They should be administrated by
            // this code only!!

            // if dir is cached or exists locally, means it exists remotely!
            // if it was deleted remotely, theres another thread wich will delete it locally,
            // so we don't have to check remotely here!!
            log_msg("\nexistsRemote -> second if - check if logFile exists and if parent folder exists -  existLocal(%s)\n", localPath(path));
            if ( existsLocal(path) ){
                if( isdir(localPath(path)) ){
                    // doCachePathDir( path );
                    log_msg("\nexistsRemote -> second if - is dir %s\n",localPath(path) );
                    return true;
                } else {
                    bool ret = exists( localPathLog(path) );
                    if ( ret && isDirCached( path ) ){
                        // we have an error that needs to be corrected! If the entry is a folder
                        // but exists as a file, something went wrong. so lets re-cache it
                        remove( localPath( path ) );
                        remove( localPathLog( path ) );
                        localFileNotExistRemove( path );
                        doCachePathDir( path );
                    }
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
                // was deleted locally in the cache only or something is wrong, so in this case,
                // we should update it from remote!
                if( exists( localPathLog(path) ) || ( ( isDirCached( path) )  && ! exists( localPathLog(path) ) ) ){
                    remove( localPathLog(path) );
                    localFileNotExistRemove( path );
                    remove( m_cache[path]["cacheReadDir"].c_str() );
                    doCachePathDir( path );
                    // after updating from remote, we return the result of existsLocal(),
                    // since the entry could very well be deleted remotely!
                    bool ret = existsLocal( path );
                    log_msg("\nexistsRemote -> third if - parent cache exists, log exists but no local path, so we re-cached and now existsLocal()=%d - %s\n", ret, path );
                    return ret;

                // we dont have a log file, but we have a dirCachedFile
                // and the local path DOESNT exist!
                }
                log_msg( "\nexistsRemote -> third if - return False - parent dir exists but no log for the entry, so it doesn't exist - %s\n", path );
                return false;
            }

            // END OF LOCAL CHECKS!!
            // Now that we exausted the local checks, there's no option but go to the remote
            // filesystem. But we MUST cache the lookup result so we don't wast time in the future!

            // first, check if the remote folder exists. If not, we already known the files in this folder
            // will not exist localy. So just cache the folder and return not_exist!!!!
            if ( ! exists( _dirname( remotePath(path) ).c_str() ) ){
                doCachePathParentDir( path );
                log_msg("\nREMOTE existsRemote -> quarth if - parent folder of path doesn't exist remotely - return False - %s\n", _dirname( remotePath(path) ).c_str() );
                // set the parent folder as non-existent too!
                localFileNotExist(_dirname(path));
                return false;
            }else{
                if( depth<1 ){
                    doCachePathParentDir( path );

                    // // if the folder exists remotely,
                    // // lets cache it to avoid future remote query on the same path
                    // log_msg("\nREMOTE existsRemote -> fifth if  - doCachePath(%s)  \n", path.c_str() );
                    // doCachePath( path );
                    // bool __isdir = isdir( localPath(path) );
                    // if( __isdir ){
                    //     log_msg("REMOTE existsRemote -> fifth if  - doCachePathDir(%s)  \n", path.c_str() );
                    //     doCachePathDir( path );
                    // }else{
                    // }

                    // now we check if the actual path exists locally, since we cached the whole parent folder.
                    bool ret = existsLocal( path );
                    log_msg("\nREMOTE existsRemote -> fifth if  - recurse returned %d \n", int(ret) );
                    if( ! ret ){
                        // set the path as non-existent, so next time we don't have to look remotely
                        localFileNotExist( path );
                    }
                    return ret;
                }
            }

            // if path doesn't exist in the remote filesystem, return notExist!
            // also set local cache of the remote state!
            if ( ! exists( remotePath(path) ) ){
                log_msg("\nREMOTE existsRemote -> sixth if - don't exist\n" );

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
                log_msg( "%s | existsLocal ->  ! exists( localPathDir(%s) ) = %d - return False\n", caller, path.c_str(), int(! exists( localPathDir(path) )) );
                return false;
            }

            // if the files exists in the cache filesystem, return EXISTS!
            if ( exists( localPath(path) ) ){
                log_msg( "%s | existsLocal ->  return exists( localPath(%s) ) = %d\n", caller, path.c_str(), int(exists( localPath(path) )) );
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
            localFileNotExist( path );
        }

        bool removeDir( string path ){
            removeFile( path );
            // this->init( path );
            // struct stat statbuf;
            // stat(path, &statbuf);
            // creat( m_cache[path]["cacheDirNotExist"].c_str(), statbuf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) );
        }

        bool fileInSync( string path, const char* caller = __builtin_FUNCTION() ){
            // returns if a files is up to date with the remote file system

            long long localSize = 0;
            long long remoteSize = 0;
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

                // we have to wait until a transfer has finished to check for the actual local size, or else
                // we get a partial size, which will return as being false, and will trigger a second transfer!!
                while( exists(localPathLock(path)) ){
                    sleep(1000);
                    log_msg( "\n !!! localFileExist: waiting for %s to vanish!\n",  localPathLock(path) );
                }

                // if its no a local created/modified file, lets check if it's up2date!
                localSize = getFileSize( localPath(path) );

                // if the log control file exists (and it should since we created it when a folder is cached),
                // compare the size of the local file with the remote files size (pre-stored in the log!)
                if ( exists( localPathLog(path) ) ){
                    remoteSize = getPathSizeFromLog( path );
                    log_msg2("\nfileInSync exist locally - [%s]  [%s] [%s] - [%ld] [%ld]\n", localPath(path),
                        m_cache[path]["cacheFileNotExist"].c_str(), localPathLog(path), localSize, remoteSize );

                    // check local size against remote size (stored in the log file)
                    if( localSize != getPathSizeFromLog( path ) ){
                        log_msg2("\ncaller(%s)\tFALSE - size is different - (need rsync) - %s \n",  caller, path.c_str() );
                        // size doesn't match, so not in sync!
                        return false;
                    }
                    // if the sizes are the same, it is in sync indeed!
                    log_msg2("\n\tTRUE - size equal - no rsync!! - %s \n", path.c_str() );
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
            _stat( remotePath( path ), &fpath_st );
            remoteSize = getFileSize( remotePath( path ) );

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
