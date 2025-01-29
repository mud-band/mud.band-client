#!/bin/bash

VERSION=0.0.7

scp ../releases/${VERSION}/linux/mudband-${VERSION}-linux-x86_64.tar.gz \
    45.33.41.192:/opt/www.mud.band/htdocs/releases/
