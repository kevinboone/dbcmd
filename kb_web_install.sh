#!/bin/bash
WEB=/home/kevin/docs/kzone5/
SOURCE=$WEB/source
TARGET=$WEB/target
make clean
(cd ..; tar cvfz $TARGET/dbcmd.tar.gz dbcmd)
cp README_dbcmd.html $SOURCE
(cd $WEB; ./make.pl dbcmd)
