#!/usr/bin/env python3

import argparse, os, sys
from pathlib import Path
from shutil import which

# Change to repository root directory
# This allows the script to work from scripts/ folder while operating on repo root
script_dir = Path(__file__).resolve().parent
repo_root = script_dir.parent if script_dir.name == 'scripts' else script_dir
os.chdir(repo_root)

if __name__ == '__main__':

	description = """\
Analyze and convert ESP32 coredump files for Yarrboard firmware.

You need to install the ESP-IDF toolkit for it to work.
Easiest way to install it through VSCode. The framework then should be installed in ~/esp/v5.x

Then run:
  chmod 755 install.sh export.sh
  ./install.sh
  . ./export.sh
  esp-coredump --version

You need to run this every time you open a new shell:

  source ~/esp/v5.5.1/esp-idf/export.sh
"""

	parser = argparse.ArgumentParser(
    description=description,
    formatter_class=argparse.RawTextHelpFormatter
	)
	parser.add_argument("--version", help="Version of the firmware, eg. 1.2.3", default="dev")
	parser.add_argument("--file", help="Path to the file.", default="coredump.bin");
	parser.add_argument("--board", help="Hardware board revision, eg. RGB_INPUT_REV_A")

	args = parser.parse_args()

	#we need a coredump file
	if not os.path.isfile(args.file):
		print(f'Error: {args.file} not found')

	# Determine ELF path
	if (args.version == "dev"):
		elf = f".pio/build/{args.board}/firmware.elf"
	# Release ELF naming from your original script
	else:
		elf = f"docs/releases/{args.board}/{args.version}.elf"

	if not Path(elf).is_file():
		print(f"Error: ELF not found: {elf}")
		sys.exit(3)

	#keep our ELF file for debugging later on....
	print("Analyzing coredump")
	cmd = f'espcoredump.py info_corefile -c "{args.file}" -t raw {elf}'
	print(cmd)
	os.system(cmd)