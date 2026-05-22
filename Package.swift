// swift-tools-version:5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "libetpan",
    platforms: [
        .iOS(.v12), .macOS(.v10_13)
    ],
    products: [
        .library(
            name: "libetpan",
            targets: ["libetpan"]),
    ],
    targets: [
        .target(name: "libetpan",
                path: ".",
                exclude: ["src/windows",
                          "src/bsd"],
                sources: ["src"],
                publicHeadersPath: "spm_include",
                cSettings: [
                    .headerSearchPath("spm_include/libetpan"),
                    .headerSearchPath("src/data-types"),
                    .headerSearchPath("src/low-level/feed"),
                    .headerSearchPath("src/low-level/imap"),
                    .headerSearchPath("src/low-level/imf"),
                    .headerSearchPath("src/low-level/maildir"),
                    .headerSearchPath("src/low-level/mbox"),
                    .headerSearchPath("src/low-level/mh"),
                    .headerSearchPath("src/low-level/mime"),
                    .headerSearchPath("src/low-level/nntp"),
                    .headerSearchPath("src/low-level/pop3"),
                    .headerSearchPath("src/low-level/smtp"),
                    .headerSearchPath("src/driver/implementation"),
                    .headerSearchPath("src/driver/interface"),
                    .headerSearchPath("src/driver/tools"),
                    .define("HAVE_LIMITS_H"),
                    .define("HAVE_UNISTD_H"),
                    .define("HAVE_SYS_MMAN_H"),
                    .define("HAVE_INTTYPES_H"),
                    .define("HAVE_CFNETWORK"),
                    .define("HAVE_SYS_PARAM_H"),
                    .define("HAVE_MEMORY_H"),
                    .define("HAVE_MMAP"),
                    .define("HAVE_NETINET_IN_H"),
                    .define("HAVE_PTHREAD_H"),
                    .define("HAVE_STDINT_H"),
                    .define("HAVE_STDLIB_H"),
                    .define("HAVE_STRINGS_H"),
                    .define("HAVE_SYS_PARAM_H"),
                    .define("HAVE_SYS_SELECT_H"),
                    .define("HAVE_SYS_STAT_H"),
                    .define("HAVE_SYS_TYPES_H"),
                    .define("HAVE_UNISTD_H"),
                    .define("HAVE_SYS_SOCKET_H"),
                    .define("HAVE_ZLIB"),
                    .define("LIBETPAN_REENTRANT"),
                    .define("PACKAGE", to: "libetpan"),
                    .define("USE_SASL", .when(platforms: [.macOS]))
                ],
                linkerSettings: [
                    .linkedLibrary("iconv"),
                    .linkedLibrary("z"),
                    .linkedLibrary("sasl2", .when(platforms: [.macOS])),
                    .linkedLibrary("c")
                ]),
        
    ],
    cLanguageStandard: .gnu11,
    cxxLanguageStandard: .cxx20
)
