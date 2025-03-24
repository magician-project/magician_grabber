#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"


cp ../../c/grabber/*.c ./
cp ../../c/grabber/*.h ./

ln -s ../../c/grabber/tactile_processor

exit 0
