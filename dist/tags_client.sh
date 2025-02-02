#!/bin/sh

NOW=`date +%Y%m%d%H%M%S`
VER=0.0.8

echo "Tags the Mud.band client v$VER-$NOW."
git tag -a client-v$VER-$NOW -m "Tags v$VER-$NOW"
git push origin client-v$VER-$NOW
