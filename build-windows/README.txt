
libEtPan!

     _________________________________________________________________

Windows Build:

This folder contains the suff needed for the Windows build.
     _________________________________________________________________

Build a version:

1. Open libetpan.sln with VC++ 2008

2. Choose configuration Debug or Release

3. Build Solution

	This will generate libetpan.dll and readmsg.exe and smtpsend.exe and the include/libetpan
folder. This folder, in combination with libetpan.lib, is needed for your
Windows applications using the libetpan.dll.

     _________________________________________________________________

Build a SSL version:

0. OpenSSL include files and binaries for Windows can be obtained at www.openssl.org/related/binaries.html.

1. Open libetpan.sln with VC++ 2008

2. Choose configuration Debug_ssl or Release_ssl

3. Create two folders on same level as libetpan folder
	3.1 3include : should contains openssl/ folder with all its includes
	3.2 3lib : should  contain openssl/ folder with the four build files (Win32) and two *.lib files.
	   - Debug_ssl configuration expects ssleay32MDd.lib and libeay32MDd.lib 
	    (MDd suffix indicates for version built for Multithreaded Debug C Runtime)
	   - Release_ssl configuration expects ssleay32MD.lib and libeay32MD.lib 
	    (MDd suffix indicates for version built for Multithreaded Release C Runtime)
		

4. Build Solution

	This will generate libetpan.dll, readmsg.exe  and smtpsend.exe and the include/libetpan
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



