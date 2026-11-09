#!/bin/bash


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"


if [ -d venv/ ]
then
echo "Found a virtual environment" 
source venv/bin/activate
else 
echo "Creating a virtual environment"
#Simple dependency checker that will apt-get stuff if something is missing
# sudo apt-get install python3-venv python3-pip
SYSTEM_DEPENDENCIES="python3-venv python3-pip"

for REQUIRED_PKG in $SYSTEM_DEPENDENCIES
do
PKG_OK=$(dpkg-query -W --showformat='${Status}\n' $REQUIRED_PKG|grep "install ok installed")
echo "Checking for $REQUIRED_PKG: $PKG_OK"
if [ "" = "$PKG_OK" ]; then

  echo "No $REQUIRED_PKG. Setting up $REQUIRED_PKG."

  #If this is uncommented then only packages that are missing will get prompted..
  #sudo apt-get --yes install $REQUIRED_PKG

  #if this is uncommented then if one package is missing then all missing packages are immediately installed..
  sudo apt-get install $SYSTEM_DEPENDENCIES  
  break
fi
done
#------------------------------------------------------------------------------

python3 -m venv venv
source venv/bin/activate
python3 -m pip install opencv-python
fi 



# Prefer the copy the top-level project already builds (src/sharedMemoryVideoBuffers.c,
# kept in sync via scripts/updateSharedMemoryMechanism.sh) instead of cloning and
# building a second, independent copy of the library here - that's what used to
# happen (via a broken "[ -d SharedMemoryVideoBuffers/Makefile ]" check, which
# tests for a directory named after a file, so it was always false and this
# branch ran on every single invocation) and it let this viewer drift onto a
# different vintage of SharedMemoryManager.py/sharedMemoryVideoBuffers.c than the
# rest of the project without anyone noticing.
if [ -f ../../libSharedMemoryVideoBuffers.so ]
then
echo "Using the already-built top-level libSharedMemoryVideoBuffers.so"
ln -sf ../../libSharedMemoryVideoBuffers.so ./libSharedMemoryVideoBuffers.so
elif [ -f libSharedMemoryVideoBuffers.so ] || [ -L libSharedMemoryVideoBuffers.so ]
then
echo "Found libSharedMemoryVideoBuffers.so"
else
echo "Top-level libSharedMemoryVideoBuffers.so not found - run 'make libSharedMemoryVideoBuffers.so' (or scripts/updateSharedMemoryMechanism.sh) from the project root first."
exit 1
fi




exit 0
