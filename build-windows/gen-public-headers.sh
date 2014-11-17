#!/bin/sh

files=""

list_headers()
{
  local filename="$1"
  
  if echo $files | grep "\[$filename\]" >/dev/null ; then
    return
  fi
  
  local path="`find . -name $filename`"
  if test "x$path" = x ; then
    return
  fi
  echo $path | sed 's/^.\//..\/src\//' | sed 's/\//\\/g'
  files="$files[$filename]"
  subfilenames="`grep '#include <libetpan/' "$path" | sed 's/^#include <libetpan\/\(.*\)>$/\1/'`"
  for include_dir in $subfilenames ; do
    list_headers $include_dir
  done
}

cd ../src
list_headers libetpan.h | sort
echo ..\\src\\windows\\win_etpan.h
echo libetpan-config.h
echo libetpan_version.h
