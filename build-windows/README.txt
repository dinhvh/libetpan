
libEtPan!

     _________________________________________________________________

Windows Build:

This folder contains the suff needed for the Windows build.
     _________________________________________________________________

Build a version:

1. Open libetpan.sln with VC++ 7.1

2. Choose configuration Debug or Release

3. Build Solution

	This will generate libetpan.dll and readmsg.exe and the include/libetpan
folder. This folder, in combinaition with libetpan.lib, is needed for your
Windows applications using the libetpan.dll.

     _________________________________________________________________

Build a SSL version:

1. Open libetpan.sln with VC++ 7.1

2. Choose configuration Debug_ssl or Release_ssl

3. create two folders on same level of libetpan folder
	- 3include : contains openssl/ folder with all its includes
	- 3lib : contains openssl/ folder with the four build files (Win32)
		- openssl/libcrypto.lib
		- openssl/libeay32.dll
		- openssl/libssl.lib
		- openssl/ssleay32.dll

4. Build Solution

	This will generate libetpan.dll and readmsg.exe and the include/libetpan
folder. This folder, in combinaition with libetpan.lib, is needed for your
Windows applications using the libetpan.dll.

     _________________________________________________________________


Copy of headers:

The include folder is build by build_headers.bat, the dependence is not based on headers
files themselves, but on a fake file, genarated after the .bat was executed (_headers_depends).
So, if you modify original headers (in src), you need to remove this file to refresh the 
includes copy folder.

     _________________________________________________________________
 
 Linker errors:

 If you are getting a missing function linker error, a new source file may have been added to the 
 src folder somewhere by another (unix based) contributor.  This now must be added to the solution
 for it to compile properly.


     _________________________________________________________________

Supported drivers:

	pop3
	imap
	nntp

     _________________________________________________________________

TODO : 

	- support mmap
	- support dirent
	- support Berkeley DB cache
 
     _________________________________________________________________


