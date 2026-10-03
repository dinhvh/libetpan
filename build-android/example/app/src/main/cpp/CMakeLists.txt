cmake_minimum_required(VERSION 3.22.1)
project(libetpanjni C)

set(PREBUILT "${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/${ANDROID_ABI}")

foreach(name etpan sasl2 ssl crypto iconv json-c curl xml2 rnp)
    add_library(${name} STATIC IMPORTED)
    set_target_properties(${name} PROPERTIES
        IMPORTED_LOCATION "${PREBUILT}/lib${name}.a")
endforeach()

add_library(etpanjni SHARED libetpan_jni.c)

target_include_directories(etpanjni PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/include/libxml")

find_library(log-lib log)
# libetpan's mailstream_compress (IMAP COMPRESS) needs zlib, shipped by the NDK.
find_library(z-lib z)

# Link order matters: consumers before providers.
target_link_libraries(etpanjni etpan rnp curl xml2 json-c sasl2 ssl crypto iconv ${z-lib} ${log-lib})
