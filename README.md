## LibEtPan

The purpose of this mail library is to provide a portable, efficient framework for different kinds of mail access: IMAP, SMTP, POP and NNTP.

It provides an API for C language.

## Features

- IMAP
- SMTP
- POP
- NNTP
- RFC822/MIME message builder
- RFC822/MIME message parser
- Maildir
- mbox
- MH

## Build instructions

### Unix

You need to install autoconf, automake and libtool.
They can be installed using [brew](http://brew.sh/).

    $ ./autogen.sh
    $ make

You can use flag --with-poll for using poll() instead of select() for checking connection status

### How to link with it

    $ gcc -c -o sample.o sample.c `pkg-config libetpan --cflags`
    $ gcc -o sample sample.o `pkg-config libetpan --libs`

### Mac / iOS

- Install Xcode and CMake. Autoconf, Automake, and Libtool are optional; when
  present, the bootstrap refreshes the checked-in autotools output.
- Configure libEtPan, initialize the dependency submodules, and build their XCFrameworks:

      $ ./build-mac/bootstrap.sh

- Open `build-mac/libetpan.xcworkspace`
- Choose the correct target "static libetpan" for Mac or "libetpan ios" for iOS.
- To build all supported slices, choose the "libetpan xcframework" target. The result is written to `build-mac/build/LibEtPan.xcframework`.
- Build

### Setup a Mac project

- Add `libetpan.xcodeproj` as sub-project
- Link with libetpan.a
- Link with `build-mac/dependencies/build/Jansson.xcframework` and `build-mac/dependencies/build/CyrusSASL.xcframework`

### Setup an iOS project

- Add `libetpan.xcodeproj` as sub-project
- Link with libetpan-ios.a
- Link with `build-mac/dependencies/build/Jansson.xcframework` and `build-mac/dependencies/build/CyrusSASL.xcframework`

### Build on Windows

- See README and Visual Studio Solution in build-windows folder

## More information

See http://etpan.org/libetpan.html for more information and examples.
