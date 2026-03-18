#!/bin/bash
THISDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$THISDIR"
cd ..


if [ -f SharedMemoryVideoBuffers/README.md ]
then
  echo "Shared Memory Repo seems to already been git cloned once.."
  cd SharedMemoryVideoBuffers
  git pull
  make
  cd ..
else
  echo "Cloning a fresh Shared Memory Repo.."
  git clone https://github.com/AmmarkoV/SharedMemoryVideoBuffers
  cd SharedMemoryVideoBuffers
  make
  cd ..
fi

cp SharedMemoryVideoBuffers/src/c/sharedMemoryVideoBuffers.c
cp SharedMemoryVideoBuffers/src/c/sharedMemoryVideoBuffers.h
cp SharedMemoryVideoBuffers/src/python/SharedMemoryManager.py

exit 0
