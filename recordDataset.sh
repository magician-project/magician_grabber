#!/bin/bash

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <name> <exposure> [extra magician_grabber args...]"
  echo
  echo "Records a 60 second dataset from the camera/arduino sensors."
  echo
  echo "Arguments:"
  echo "  <name>      Label for this recording; the output directory is named <name>_<exposure>"
  echo "  <exposure>  Camera exposure value to use for the capture"
  echo "  [extra...]  Any additional arguments are appended to the magician_grabber command"
  echo
  echo "Example: $0 mySession 5000"
  echo "Example: $0 mySession 5000 --gain 2 --verbose"
  exit 1
fi

NAME="$1"
EXPOSURE="$2"
OUTPUT="${NAME}_${EXPOSURE}"
shift 2

./magician_grabber --camera  --distance --arduino /dev/ttyACM0 --output "$OUTPUT" --exposure "$EXPOSURE" --time 60 --pico2 "$@"

# Ask whether to upload the recorded dataset to the remote machine
read -r -p "Dataset '$OUTPUT' recorded. Upload it now? [y/N] " ANSWER
case "$ANSWER" in
  [yY]|[yY][eE][sS])
    echo "Uploading '$OUTPUT' ..."
    scp -r -P 2222 "$OUTPUT/" ammar@ammar.gr:/media/ammar/FastDatasets/Magician/CameraV2Datasets
    ;;
  *)
    echo "Skipping upload."
    ;;
esac

exit 0
