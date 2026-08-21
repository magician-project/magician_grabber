#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"


cp ../../c/grabber/*.c src/
cp ../../c/grabber/*.h src/

ln -s ../../../c/grabber/tactile_processor src/tactile_processor

exit 0
