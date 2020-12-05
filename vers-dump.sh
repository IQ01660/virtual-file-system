#!/usr/bin/bash

# Assume the first arg $1 is the name of the file, e.g. foo.txt
# This means we should go into ./stg/.vers/foo.txt_hist/
# where we have a list of all versions of foo.txt
HIST_FOLDER_PATH="./stg/.vers/$1_hist/"

# dumping all the files in that folder into the project folder
# we should have files like: foo.txt,v - where v is a version number
echo "Dumping all the versions of $1 into the project folder..."

# getting into the folder with all file versions
cd $HIST_FOLDER_PATH

# going through every snapshot file and copying it to the project folder
for SNAPSHOT in *; do
	cp ${SNAPSHOT} ../../../${SNAPSHOT}
done

echo "Now you can do ls -l to see the files dumped into the project folder"





