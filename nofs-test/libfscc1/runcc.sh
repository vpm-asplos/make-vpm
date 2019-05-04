# CC1=strace -e trace=file /usr/lib64/gcc/x86_64-suse-linux/7/cc1
# CC1=/usr/lib64/gcc/x86_64-suse-linux/7/cc1
# CC1=strace -e trace=file ./cc1
CC1=./cc1

${CC1} lfs.c -dumpbase lfs.c -mtune=generic -march=x86-64 -auxbase lfs-fstack-protector-strong -Wformat -Wformat-security -DUSER_ALLOCATE_SPACE p
${CC1} lfs_error.c -dumpbase lfs_error.c  -mtune=generic -march=x86-64 -auxbase lfs-fstack-protector-strong -Wformat -Wformat-security p
${CC1} bitmap.c -dumpbase bitmap.c  -mtune=generic -march=x86-64 -auxbase lfs-fstack-protector-strong -Wformat -Wformat-security p
${CC1} inode.c -dumpbase inode.c  -mtune=generic -march=x86-64 -auxbase lfs-fstack-protector-strong -Wformat -Wformat-security p
${CC1} sync.c -dumpbase sync.c  -mtune=generic -march=x86-64 -auxbase lfs-fstack-protector-strong -Wformat -Wformat-security p
${CC1} file.c -dumpbase file.c  -mtune=generic -march=x86-64 -auxbase lfs-fstack-protector-strong -Wformat -Wformat-security p
${CC1} test.c -dumpbase test.c  -mtune=generic -march=x86-64 -auxbase lfs-fstack-protector-strong -Wformat -Wformat-security p

