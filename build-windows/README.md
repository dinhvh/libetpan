## libEtPan on Windows ##

All the provided binaries are compiled in release mode.
For the debug mode, you need to download the repositories and compile them.

### Binary ###

In case you just need a binary build of libEtPan:
- [libEtPan](http://d.etpan.org/mailcore2-deps/libetpan-win32/)

Also, you'll need all the dependencies, download the most recent binary builds in:

- [Cyrus SASL](http://d.etpan.org/mailcore2-deps/cyrus-sasl-win32/)
- [zlib](http://d.etpan.org/mailcore2-deps/zlib-win32/)
- [OpenSSL](http://d.etpan.org/mailcore2-deps/misc-win32/)

### Build using Visual Studio 2013 ###

You'll need all the dependencies, download the most recent binary builds in:

- [zlib](http://d.etpan.org/mailcore2-deps/zlib-win32/)
- [OpenSSL](http://d.etpan.org/mailcore2-deps/misc-win32/)

#### Instructions for zlib ####

- copy `include`, `lib` and `lib64` folders to `libetpan/third-party`.

#### openssl ####

- copy `bin`, `bin64`, `include`, `lib` and `lib64` to `mailcore2/Externals`.

As a result, in `Externals` folder, you should have the following folders: `include`, `lib`, `lib64`, `bin` and `bin64`.

In `libetpan/build-windows`, using Visual Studio 2013, open `libetpan.sln`.
Then, build.

Public headers will be located in `libetpan/build-windows/include`.
