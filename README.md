# Virtual File System

## Description

Here we create our own Virtual File System (VFS) using FUSE—a
tool that allows us to run file system code as a regular program (a.k.a. in userspace). When
a FUSE program is run, it creates a file system at a mount point provided by the user, and
then makes its virtual file system appear within that mount point. All operations performed
within the subdirectories and files within that mount point are passed, by the kernel, onto
the FUSE program to handle.

### mirrorfs
When run, mirrorfs.c takes the file system mounted at some point by the user and creates 
a virtual file system that completely mirrors the mounted folder
and any actions performed in there

To run first:
```
$ make mirrorfs
```
and then,
```
$ mkdir stg mnt
$ ./mirrorfs ${PWD}/stg ${PWD}/mnt
```

To unmount:
```
$ fusermount -u ${PWD}/mnt
```

### caesarfs
The caesarfs.c code does almost the same thing as the mirrorfs.c except for the fact that
all the text written and read from files gets encrypted and decrypted using a Caesar Cipher.

To run do the following:
```
$ ./caesarfs ${PWD}/stg ${PWD}/mnt 3
```

### versfs
The code in verfsfs.c allows the virtual file system to perform version control on any files created and changed 
in the mounted file system by storing all the versions of files in the VFS.

```
$ ./versfs ${PWD}/stg ${PWD}/mnt
```

After creating and changing files in the mounted folder, one can dump all versions of a specific file into the project directory
by following the Version Dump Instructions shown below.

# Version Dump Instructions

In order to perform the version dump,
navigate to the project folder sysproj-8
and then execute the following:

```
$ bash vers-dump.sh <filename.txt>
```

For example, to dump all the versions of foo.txt

```
$ bash vers-dump.sh foo.txt
```

Make sure the name of the file matches with 
that put in the mnt/ earlier.

After executing the command as shown above, you should see some messages
printed in the terminal, so you can just do:

```
$ ls -l
```
and see the version files dumped into the project folder.
