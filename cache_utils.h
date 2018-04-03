/*
  HradecFS
  Copyright (C) 2018 Roberto Hradec <me@hradec.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer".

*/


#include "fileUtils.h"

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <cstdarg>
#include <thread>
#include <sstream>
#include <time.h>
#include <boost/algorithm/string/replace.hpp>

using namespace std;

typedef struct stat _stat;

static pthread_mutex_t mutex;

std::string right(std::string const& source, size_t const length) {
  if (length >= source.size()) { return source; }
  return source.substr(source.size() - length);
}

std::string left(std::string const& source, size_t const length) {
  if (length >= source.size()) { return source; }
  return source.substr(0, length);
}

vector<string> split(const string &s, char delim) {
    stringstream ss(s);
    string item;
    vector<string> tokens;
    while (getline(ss, item, delim)) {
        tokens.push_back(item);
    }
    return tokens;
}

//missing string printf
//this is safe and convenient but not exactly efficient
std::string _format(const char* fmt, ...){
    int size = 4096;
    char* buffer = 0;
    buffer = new char[size];
    va_list vl;
    va_start(vl, fmt);
    int nsize = vsnprintf(buffer, size, fmt, vl);
    if(size<=nsize){ //fail delete buffer and try again
        delete[] buffer;
        buffer = 0;
        buffer = new char[nsize+1]; //+1 for /0
        nsize = vsnprintf(buffer, size, fmt, vl);
    }
    std::string ret(buffer);
    va_end(vl);
    delete[] buffer;
    return ret;
}

bool _replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}


string _dirname(string source)
{
    if (source.size() <= 1) //Make sure it's possible to check the last character.
    {
        return source;
    }
    if (*(source.rbegin() + 1) == '/') //Remove trailing slash if it exists.
    {
        source.pop_back();
    }
    source.erase(std::find(source.rbegin(), source.rend(), '/').base(), source.end());
    return boost::replace_all_copy( source, "//","/" );
}

// trim from end of string (right)
inline std::string rtrim(std::string s, const char* t )
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from beginning of string (left)
inline std::string& ltrim(std::string& s, const char* t )
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

int getdir (string dir, vector<string> &files)
{
    log_msg( "getdir( %s )\n", dir.c_str() );

    string cdir = rtrim(dir,"/");
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) {
        // cout << "Error(" << errno << ") opening " << dir << endl;
        log_msg( "\tgetdir %s ERROR: %d\n", dir.c_str(), errno);
        return errno;
    }

    while ((dirp = readdir(dp)) != NULL) {
        if( ! (string(dirp->d_name) == ".." ||  string(dirp->d_name) == ".") )
            files.push_back(cdir+"/"+string(dirp->d_name));
    }
    closedir(dp);
    return 0;
}




char** str_split( char* str, char delim, int* numSplits )
{
    char** ret;
    int retLen;
    char* c;

    if ( ( str == NULL ) ||
        ( delim == '\0' ) )
    {
        /* Either of those will cause problems */
        ret = NULL;
        retLen = -1;
    }
    else
    {
        retLen = 0;
        c = str;

        /* Pre-calculate number of elements */
        do
        {
            if ( *c == delim )
            {
                retLen++;
            }

            c++;
        } while ( *c != '\0' );

        ret = (char **)malloc( ( retLen + 1 ) * sizeof( *ret ) );
        ret[retLen] = NULL;

        c = str;
        retLen = 1;
        ret[0] = str;

        do
        {
            if ( *c == delim )
            {
                ret[retLen++] = &c[1];
                *c = '\0';
            }

            c++;
        } while ( *c != '\0' );
    }

    if ( numSplits != NULL )
    {
        *numSplits = retLen;
    }

    return ret;
}
