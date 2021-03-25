#!/bin/sh
cd dependencies
for script in prepare-*.sh ; do
	echo running $script
	sh "$script"
done
