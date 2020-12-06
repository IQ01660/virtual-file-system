# Version Dump Instructions

In order to perform the version dump,
navigate to the project folder sysproj-8
and then execute the following:

```
bash vers-dump.sh <filename.txt>
```

For example, to dump all the versions of foo.txt

```
bash vers-dump.sh foo.txt
```

Make sure the name of the file matches with 
that put in the mnt/ earlier.

After executing the command as shown above, you should see some messages
printed in the terminal, so you can just do:

```
ls -l
```
and see the version files dumped into the project folder.