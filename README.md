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

    $ ./configure
    $ make

### How to link with it

    $ gcc -c -o sample.o sample.c `libetpan-config --cflags`
    $ gcc -o sample sample.o `libetpan-config --libs`

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

## More information

See http://etpan.org/libetpan.html for more information and examples.
