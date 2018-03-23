# HradecFS 
---


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
  
  I'll try to describe it better in the future... ;)

  Thanks lots, Mr. Joseph J. Pfeiffer!!
