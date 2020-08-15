## LibEtPan

The purpose of this mail library is to provide a portable, efficient framework for different kinds of mail access: IMAP, SMTP, POP and NNTP.

It provides an API for C language.

[![Build Status](https://travis-ci.org/dinhviethoa/libetpan.png?branch=master)](https://travis-ci.org/dinhviethoa/libetpan)
[![Code Quality: Cpp](https://img.shields.io/lgtm/grade/cpp/g/dinhviethoa/libetpan.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/dinhviethoa/libetpan/context:cpp)
[![Total Alerts](https://img.shields.io/lgtm/alerts/g/dinhviethoa/libetpan.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/dinhviethoa/libetpan/alerts)

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

- Download Xcode
- Open `build-mac/libetpan.xcodeproj`
- Choose the correct target "static libetpan" for Mac or "libetpan ios" for iOS.
- Build

### Setup a Mac project

- Add `libetpan.xcodeproj` as sub-project
- Link with libetpan.a

### Setup an iOS project

- Add `libetpan.xcodeproj` as sub-project
- Link with libetpan-ios.a
- Set "Other Linker Flags": `-lsasl2`

### Build on Windows

- See README and Visual Studio Solution in build-windows folder

## More information

See http://etpan.org/libetpan.html for more information and examples.
