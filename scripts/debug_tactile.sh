#!/bin/bash
THISDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$THISDIR"
  
cd ..
 
valgrind --tool=memcheck --leak-check=yes --show-reachable=yes --track-origins=yes --num-callers=20 --track-fds=yes ./magician_grabber_tactile --all --output /media/ammar/MAGICIAN16TB/Magician/tactile --time 3 --nokb $@ 2>error.txt
cat error.txt 

exit 0
