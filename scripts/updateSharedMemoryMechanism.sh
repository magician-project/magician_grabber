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



if [ -f SharedMemoryVideoBuffers/README.md ]
then
  cp SharedMemoryVideoBuffers/src/c/sharedMemoryVideoBuffers.c src/
  cp SharedMemoryVideoBuffers/src/c/sharedMemoryVideoBuffers.h src/
  cp SharedMemoryVideoBuffers/src/python/SharedMemoryManager.py ./

  # src/viewer/ keeps its own copy of the Python wrapper (and needs
  # SharedMemoryServer.py too) - refresh those as well so it can't silently
  # drift onto an older vintage than the rest of the project.
  cp SharedMemoryVideoBuffers/src/python/SharedMemoryManager.py src/viewer/
  cp SharedMemoryVideoBuffers/src/python/SharedMemoryServer.py src/viewer/

  # Copying fresh source doesn't rebuild anything by itself - the previous
  # version of this script stopped here, which is how the top-level
  # libSharedMemoryVideoBuffers.so ended up missing symbols (getVideoFrameTimestamp/
  # setVideoFrameTimestamp) that src/sharedMemoryVideoBuffers.c already had.
  echo "Rebuilding libSharedMemoryVideoBuffers.so from the refreshed source..."
  make libSharedMemoryVideoBuffers.so
else
  echo "Failed updating shared memory code"
  exit 1
fi

exit 0
