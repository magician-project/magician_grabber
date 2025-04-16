#!/bin/bash
THISDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$THISDIR"
cd ..


GROUP="tty"
if id -nG "$USER" | grep -qw "$GROUP"; then
    echo $USER belongs to $GROUP
else
    echo $USER does not belong to $GROUP
    sudo usermod -a -G $GROUP $USER
fi

GROUP="dialout"
if id -nG "$USER" | grep -qw "$GROUP"; then
    echo $USER belongs to $GROUP
else
    echo $USER does not belong to $GROUP
    sudo usermod -a -G $GROUP $USER
fi



#Build tactile_processor libraries
cd tactile_processor
./build.sh

cd "$THISDIR"
cd ..

if [ -f aravis/meson.build ]
then
echo "ARAVIS seems to already exist.."
else
  echo "Cloning ARAVIS"
  sudo apt install meson ninja-build gettext
  git clone https://github.com/AravisProject/aravis
  cd aravis && meson build && cd build && ninja && sudo ninja install
fi

make

exit 0
