#!/bin/bash

VERSION=0.0.6

scp ../releases/${VERSION}/windows/x86/mudband-${VERSION}-windows-x86.zip \
    ../releases/${VERSION}/windows/x86_64/mudband-${VERSION}-windows-x86_64.zip
    45.33.41.192:/opt/www.mud.band/htdocs/releases/
