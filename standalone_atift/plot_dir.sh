#!/usr/bin/env bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Exit on error, unset vars, or pipe failure
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <directory> <extension>"
    exit 1
fi

SEARCH_DIR="$1"
EXT="$2"

PYTHON_SCRIPT="plotATICSV.py"  # change to your python script name

if [ ! -d "$SEARCH_DIR" ]; then
    echo "Error: Directory '$SEARCH_DIR' does not exist."
    exit 1
fi

# Activate venv
source venv/bin/activate

# Recursively find and process files
find "$SEARCH_DIR" -type f -name "*$EXT" -print0 | while IFS= read -r -d '' file; do
    echo "Processing: $file"
    python3 "$PYTHON_SCRIPT" "$file" -o "$file-vis.mp4" --realtime


done

