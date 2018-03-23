/*
  HradecFS
  Copyright (C) 2018 Roberto Hradec <me@hradec.com>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  HradecFS is derived from "Big Brother File System by Joseph J. Pfeiffer".

*/


typedef struct stat _stat;
static pthread_mutex_t mutex;


//missing string printf
//this is safe and convenient but not exactly efficient
inline std::string _format(const char* fmt, ...){
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
    string cdir = rtrim(dir,"/");
    DIR *dp;
    struct dirent *dirp;
    if((dp  = opendir(dir.c_str())) == NULL) {
        cout << "Error(" << errno << ") opening " << dir << endl;
        return errno;
    }

    while ((dirp = readdir(dp)) != NULL) {
        if( ! (string(dirp->d_name) == ".." ||  string(dirp->d_name) == ".") )
            files.push_back(cdir+"/"+string(dirp->d_name));
    }
    closedir(dp);
    return 0;
}
