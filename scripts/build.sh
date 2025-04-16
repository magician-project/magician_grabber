#!/bin/bash
THISDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$THISDIR"
cd ..

#This needs to be removed
sudo apt remove brltty

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


#Add export LD_LIBARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/x86_64-linux-gnu if not present
TARGET_DIR="/usr/local/lib/x86_64-linux-gnu"
EXPORT_LINE="export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:$TARGET_DIR"

# Check if the export line is already in .bashrc
if grep -Fxq "$EXPORT_LINE" ~/.bashrc; then
    echo "LD_LIBRARY_PATH already includes $TARGET_DIR in .bashrc"
else
    echo "$EXPORT_LINE" >> ~/.bashrc
    echo "Added $TARGET_DIR to LD_LIBRARY_PATH in .bashrc"
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





cd "$THISDIR"
cd ..
make

exit 0
