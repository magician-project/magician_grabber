#!/bin/bash


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"



if [ -d venv/ ]
then
echo "Found a virtual environment" 
source venv/bin/activate
else
./build.sh 
fi



# Name of the process to watch
process_name="magician_grab"

# Run your Python script in the background
python3 viewer.py &
python_pid=$!

echo "Started python3 my_script.py with PID $python_pid"
echo "Waiting for process '$process_name' to finish..."

# Loop until the process is no longer running
#while pgrep -x "$process_name" >/dev/null; do
while ps -A | grep -w "$process_name" | grep -v "grep" >/dev/null; do
    sleep 1
done

echo "Process '$process_name' has exited."

# Optionally, terminate the Python script
echo "Terminating python script with PID $python_pid"
kill "$python_pid" 2>/dev/null

# Optional: wait for python process to actually terminate
wait "$python_pid" 2>/dev/null





exit 0
