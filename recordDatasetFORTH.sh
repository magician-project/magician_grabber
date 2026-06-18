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

# Print the average hovering distance of a dataset by averaging the Distance1,
# Distance2 and Distance3 columns of <dataset>/controller.csv over all rows.
# Columns are resolved by header name so the script keeps working if the CSV
# column order ever changes.
print_avg_distance() {
  local csv="$1/controller.csv"
  if [ ! -f "$csv" ]; then
    echo "Average hovering distance: N/A ('$csv' not found)"
    return
  fi
  awk -F',' '
    NR == 1 {
      for (i = 1; i <= NF; i++) {
        if ($i == "Distance1") d1 = i
        if ($i == "Distance2") d2 = i
        if ($i == "Distance3") d3 = i
      }
      if (!d1 || !d2 || !d3) {
        print "Average hovering distance: N/A (Distance columns not found)"
        exit 1
      }
      next
    }
    { sum += $d1 + $d2 + $d3; n += 3 }
    END {
      if (n > 0)
        printf "Average hovering distance: %.2f (over %d samples)\n", sum / n, n
      else
        print "Average hovering distance: N/A (no data rows)"
    }
  ' "$csv"
}

# Clean up any leftovers from a previous (possibly interrupted) run so the new
# recording does not get mixed with stale files. This covers the output dir
# itself plus the temp/backup folders that compressDataset.py may leave behind.
for STALE in "$OUTPUT" ".${OUTPUT}.tmp_compress" ".${OUTPUT}.backup_old"; do
  if [ -e "$STALE" ]; then
    echo "Removing leftover '$STALE' from a previous run ..."
    rm -rf -- "$STALE"
  fi
done

./magician_grabber --camera  --distance --arduino /dev/ttyACM0 --output "$OUTPUT" --exposure "$EXPOSURE" --time 60 --pico2 "$@"


echo "Compressing '$OUTPUT' (converting .pnm -> .png) ..."
../magician_grabber_annotator/venv/bin/python ../magician_grabber_annotator/compressDataset.py "$OUTPUT"

# Report the average hovering distance before asking about upload
print_avg_distance "$OUTPUT"

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
