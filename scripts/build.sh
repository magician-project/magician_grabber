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
  sudo apt install libxml2-dev libglib2.0-dev cmake libusb-1.0-0-dev gobject-introspection libgtk-3-dev gtk-doc-tools  xsltproc libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-good1.0-dev libgirepository1.0-dev 
  git clone https://github.com/AravisProject/aravis
  cd aravis && meson build && cd build && ninja && sudo ninja install
fi

make

exit 0
