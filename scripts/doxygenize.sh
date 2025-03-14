#!/bin/bash

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <source_directory>"
  exit 1
fi

SOURCE_DIR="$1"
OUTPUT_DIR="doxygen_outputs"
LLAMACPP="/home/ammar/Documents/3dParty/llama.cpp/build/bin"
MODEL="/media/ammar/games2/DeepSeek-R1-Distill-Qwen-14B-Q5_K_M.gguf"

mkdir -p "$OUTPUT_DIR"
echo "Processing C source files in $SOURCE_DIR..."

MESSAGE="""Please analyze the following C source code and generate clear and detailed Doxygen-style comments for functions, structures, and important variables. Respond by outputting the modified C code with the Doxygen comments included above relevant entities. Do not alter the actual source code. The format should follow standard Doxygen documentation conventions. C Source Code:"""

for file in "$SOURCE_DIR"/*.c; do
  if [ -f "$file" ]; then
    filename=$(basename -- "$file")
    output_file="$OUTPUT_DIR/${filename%.c}-doxygen.c"
    temp_input="$OUTPUT_DIR/temp.in"
    
    echo "$MESSAGE" > "$temp_input"
    cat "$file" >> "$temp_input"
    
    echo "Processing $file with Llama.cpp..."
    $LLAMACPP/llama-cli -m "$MODEL" -c 512 -b 1024 -n 512 --keep 48 --repeat_penalty 1.0 --color -r "User:" -f "$temp_input" > "$output_file"
    
    echo "Doxygenized file saved as $output_file"
  fi
done

rm -f "$OUTPUT_DIR/temp.in"
echo "Processing complete. All Doxygenized files are in $OUTPUT_DIR."


exit 0
