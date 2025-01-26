#!/bin/sh

NOW=`date +%Y%m%d%H%M%S`
VER=0.0.6

echo "Tags the Mudband v$VER-$NOW."
git pull
git tag -a client-v$VER-$NOW -m "Tags v$VER-$NOW"
git pull
git push
git push origin client-v$VER-$NOW
