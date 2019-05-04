#!/usr/bin/env bash

remove() {
    if [ -f $1 ]; then
	rm $1
    fi
}

check_exist_link() {
    if [ -f $1 ]; then
	ln -s $1 .
    else
	echo "I cannot find specified executable: $1."
	exit 1
    fi
}

# Step 0: remove the three executables and rebuild the link
remove "./main"
remove "./make"
remove "./cc1"
check_exist_link "../../../libfs/main"
check_exist_link "../../../make-nofs/make"
check_exist_link "../../../gcc-nofs/bld/gcc/cc1"

# Step 1: load
#./main
# Step 2: make
#./make
# 
