import re
import os
import argparse
from pathlib import Path

def c_array_to_bin(input_file, output_file):
    with open(input_file, "r") as f:
        text = f.read()

    print("Step 1: file loaded")

    # 🔥 Remove all /* ... */ comments FIRST
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)

    # Extract the first const uint8_t array (works with any variable name)
    match = re.search(
        r'const\s+uint8_t\s+\w+\s*(?:\[.*?\])+\s*=\s*\{([\s\S]*?)\};',
        text
    )

    print("Step 2: searching for uint8_t array...")
    if not match:
        print("FAILED: Could not find uint8_t array")
        return

    array_text = match.group(1)
    print("Found array block")

    # Extract numbers — supports both decimal (123) and hex (0xAB)
    numbers = re.findall(r'\b(?:0[xX][0-9a-fA-F]+|\d+)\b', array_text)

    print("Step 3: extracting numbers...")
    print("Count:", len(numbers))
    print("First 20:", numbers[:20])

    byte_values = [int(n, 0) for n in numbers]

    if len(byte_values) % (140 * 144) != 0:
        print(f"WARNING: {len(byte_values)} values is not a multiple of 20160 (140x144)")
    else:
        num_frames = len(byte_values) // (140 * 144)
        print(f"Frames detected: {num_frames}")

    # Write binary
    with open(output_file, "wb") as f:
        f.write(bytearray(byte_values))

    print("Binary written")
    print("File size:", os.path.getsize(output_file))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Convert a generated C globe data file to raw binary."
    )
    parser.add_argument("input", help="Path to the *_data.c file")
    parser.add_argument(
        "-o", "--output", default=None,
        help="Output .bin path (default: <input_stem>.bin in current directory)"
    )
    args = parser.parse_args()

    output = args.output or (Path(args.input).stem + ".bin")
    c_array_to_bin(args.input, output)