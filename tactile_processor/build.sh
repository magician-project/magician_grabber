#!/bin/bash
THISDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$THISDIR"


#Simple dependency checker that will apt-get stuff if something is missing
# sudo apt-get install build-essential cmake libopencv-dev libjpeg-dev libpng-dev freeglut3-dev libglew-dev libpthread-stubs0-dev
SYSTEM_DEPENDENCIES="libfftw3-dev"

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


if [ -f iir1/CMakeLists.txt ]
then
echo "IIR1 seems to already exist.."
else
  echo "Cloning IIR1"
  echo
  #echo -n " (Y/N)?"
  #read answer
  answer="Y"
  if test "$answer" != "N" -a "$answer" != "n";
  then 
    git clone https://github.com/berndporr/iir1
    ln -s iir1/iir
    cd iir1 
    mkdir build
    cd build
    cmake ..
    make -j5
    cd ..
    cd ..
  fi
fi


#Debug
#g++ TactileProcessor.cpp -D_GNU_SOURCE -O0 -g3 -fno-omit-frame-pointer -pg -Wstrict-overflow -fsanitize=address -fPIE -fPIC -Wno-unused-function -march=native -mtune=native  -o test -lfftw3 -L: iir1/build/libiir_static.a 

#Release
g++ TactileProcessor.cpp -D_GNU_SOURCE -O2  -fno-omit-frame-pointer -Wstrict-overflow -fsanitize=address -fPIE -fPIC -Wno-unused-function -march=native -mtune=native -o TactileProcessorTester -lfftw3 -L: iir1/build/libiir_static.a 

exit 0
