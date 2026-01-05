#!/usr/bin/env python3

import argparse, os, json, re, sys, subprocess, glob
from pathlib import Path

def generate_espwebtools_manifest(board_name, chip_family, version, firmware_url_base):
	"""Generate ESP Web Tools manifest.json for a board"""
	manifest = {
		"name": f"{board_name} Firmware",
		"version": version,
		"home_assistant_domain": "yarrboard",
		"new_install_prompt_erase": False,
		"builds": [
			{
				"chipFamily": chip_family,
				"parts": [
					{
						"path": "bootloader.bin",
						"offset": 4096
					},
					{
						"path": "partitions.bin",
						"offset": 32768
					},
					{
						"path": "boot_app0.bin",
						"offset": 57344
					},
					{
						"path": "firmware.bin",
						"offset": 65536
					}
				]
			}
		]
	}
	return manifest

# Change to repository root directory
# This allows the script to work from scripts/ folder while operating on repo root
script_dir = Path(__file__).resolve().parent
repo_root = script_dir.parent if script_dir.name == 'scripts' else script_dir
os.chdir(repo_root)

#what boards / build targets to include in the release
release_config_path = Path("releases/config.json")
if not release_config_path.exists():
	print("🔴 releases/config.json does not exist 🔴")
	sys.exit(1)

try:
	with open(release_config_path, "r") as f:
		release_config = json.load(f)
		boards = release_config.get("boards", [])
		firmware_url_base = release_config.get("firmware_url_base", "")
		signing_key_path = release_config.get("signing_key_path", "")
		if not boards:
			print("🔴 No boards found in release_config.json 🔴")
			sys.exit(1)
		if not firmware_url_base:
			print("🔴 No firmware_url_base found in release_config.json 🔴")
			sys.exit(1)
		if not signing_key_path:
			print("🔴 No signing_key_path found in release_config.json 🔴")
			sys.exit(1)

		# Validate board configuration format
		for board in boards:
			if isinstance(board, dict):
				if "name" not in board:
					print("🔴 Board configuration missing 'name' field 🔴")
					sys.exit(1)
				if "chip_family" not in board:
					print("🔴 Board configuration missing 'chip_family' field 🔴")
					sys.exit(1)
			else:
				print("🔴 Board configuration must be an object with 'name' and 'chip_family' fields 🔴")
				sys.exit(1)
except json.JSONDecodeError as e:
	print(f"🔴 Error parsing release_config.json: {e} 🔴")
	sys.exit(1)
except Exception as e:
	print(f"🔴 Error reading release_config.json: {e} 🔴")
	sys.exit(1)

if __name__ == '__main__':

	# --- parse arguments ---
	parser = argparse.ArgumentParser(description="Build Yarrboard firmware release")

	parser.add_argument(
		"--test", action="store_true",
		help="Run in test mode (do not actually do anything, but print the results)")

	parser.add_argument(
			"--publish",
			action="store_true",
			help="Publish the GitHub release automatically"
	)

	args = parser.parse_args()

	test_mode = args.test  # boolean True/False

	#look up our version #
	version = False
	dev_mode = False

	#check our config file for info.
	config_path = Path("src/config.h")
	library_path = Path("src/YarrboardVersion.h")
	if config_path.exists():
		file = open(config_path, "r")
		for line in file:
			#version lookup
			result = re.search(r'YB_FIRMWARE_VERSION "(.*)"', line)
			if result:
				version = result.group(1)
				break
	elif library_path.exists():
		file = open(library_path, "r")
		major = minor = patch = None
		for line in file:
			# Look for YARRBOARD_VERSION_MAJOR
			result = re.search(r'#define\s+YARRBOARD_VERSION_MAJOR\s+(\d+)', line)
			if result:
				major = result.group(1)

			# Look for YARRBOARD_VERSION_MINOR
			result = re.search(r'#define\s+YARRBOARD_VERSION_MINOR\s+(\d+)', line)
			if result:
				minor = result.group(1)

			# Look for YARRBOARD_VERSION_PATCH
			result = re.search(r'#define\s+YARRBOARD_VERSION_PATCH\s+(\d+)', line)
			if result:
				patch = result.group(1)

		if major and minor and patch:
			version = f"{major}.{minor}.{patch}"

 	#only proceed if we found the version
	if not version:
		print("🔴 YB_FIRMWARE_VERSION not #defined in src/config.h 🔴")
		sys.exit(1)   # bail out

 	#turn off dev mode
	if dev_mode:
		print("🔴 YB_IS_DEVELOPMENT set to true in src/config.h 🔴")
		sys.exit(1)   # bail out

	#check that we are on the "main" branch
	branch = subprocess.check_output(
		["git", "rev-parse", "--abbrev-ref", "HEAD"],
		text=True
	).strip()
	if branch != "main":
		print(f"⚠️ You are on '{branch}', not 'main' ⚠️")

	config = []

	# get only the latest version section from CHANGELOG.md
	with open("CHANGELOG.md", "r") as f:
		content = f.read()

	# Extract the *first* version block
	# This grabs:
	#   ## Version <version> or ### v<version> etc.
	#   ...lines...
	# up until the next heading at the same level OR end of file
	pattern = rf"(#+\s*(?:Version\s+)?v?{re.escape(version)}(?:\n(?!#).*)*)"
	m = re.search(pattern, content, re.MULTILINE)
	if not m:
		print("🔴 Could not extract latest version block from CHANGELOG.md  Needs this format: ## Version x.y.z 🔴")
		sys.exit(1)

	changelog = m.group(1).strip()

	print (f'Making firmware release for version {version}')

	for board_config in boards:
		board_name = board_config['name']
		chip_family = board_config['chip_family']

		print (f'Building {board_name} firmware for {chip_family}')

		#make our firmware.json entry
		bdata = {}
		bdata['type'] = board_name
		bdata['version'] = version
		bdata['url'] = f'{firmware_url_base}{board_name}/{board_name}-{version}.bin'
		bdata['changelog'] = changelog
		config.append(bdata)

		#build the firmware
		cmd = f'pio run -e "{board_name}" -s'
		if test_mode:
			print (cmd)
		else:
			os.system(cmd)

		#sign the firmware
		cmd = f'openssl dgst -sign {signing_key_path} -keyform PEM -sha256 -out .pio/build/{board_name}/firmware.sign -binary .pio/build/{board_name}/firmware.bin'
		if test_mode:
			print (cmd)
		else:
			os.system(cmd)

		#combine the signatures
		cmd = f'cat .pio/build/{board_name}/firmware.sign .pio/build/{board_name}/firmware.bin > .pio/build/{board_name}/signed.bin'
		if test_mode:
			print (cmd)
		else:
			os.system(cmd)

		#create board-specific releases directory
		board_dir = f'releases/{board_name}'
		if test_mode:
			print(f'mkdir -p {board_dir}')
		else:
			os.makedirs(board_dir, exist_ok=True)

		#copy our fimrware to releases directory
		cmd = f'cp .pio/build/{board_name}/signed.bin {board_dir}/{board_name}-{version}.bin'
		if test_mode:
			print (cmd)
		else:
			os.system(cmd)

		#keep our ELF file for debugging later on....
		cmd = f'cp .pio/build/{board_name}/firmware.elf {board_dir}/{board_name}-{version}.elf'
		if test_mode:
			print (cmd)
		else:
			os.system(cmd)

		# --- ESP Web Tools support ---
		print(f'Generating ESP Web Tools files for {board_name}')

		# Create espwebtools directory
		espwebtools_dir = f'releases/{board_name}-{version}-espwebtools'
		if test_mode:
			print(f'mkdir -p {espwebtools_dir}')
		else:
			os.makedirs(espwebtools_dir, exist_ok=True)

		# Copy firmware binary (unsigned version for ESP Web Tools)
		cmd = f'cp .pio/build/{board_name}/firmware.bin {espwebtools_dir}/firmware.bin'
		if test_mode:
			print(cmd)
		else:
			os.system(cmd)

		# Copy bootloader
		bootloader_path = f'.pio/build/{board_name}/bootloader.bin'
		if test_mode:
			print(f'cp {bootloader_path} {espwebtools_dir}/bootloader.bin')
		else:
			if Path(bootloader_path).exists():
				os.system(f'cp {bootloader_path} {espwebtools_dir}/bootloader.bin')
			else:
				print(f'⚠️  Warning: bootloader.bin not found at {bootloader_path}')

		# Copy partitions
		partitions_path = f'.pio/build/{board_name}/partitions.bin'
		if test_mode:
			print(f'cp {partitions_path} {espwebtools_dir}/partitions.bin')
		else:
			if Path(partitions_path).exists():
				os.system(f'cp {partitions_path} {espwebtools_dir}/partitions.bin')
			else:
				print(f'⚠️  Warning: partitions.bin not found at {partitions_path}')

		# Copy boot_app0.bin from Arduino ESP32 framework
		# This file is needed for OTA partition configurations
		boot_app0_locations = [
			Path.home() / '.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin',
			Path.home() / '.platformio/packages/framework-arduinoespressif32@src-*/tools/partitions/boot_app0.bin'
		]
		boot_app0_copied = False
		for boot_app0_path in boot_app0_locations:
			if test_mode:
				print(f'cp {boot_app0_path} {espwebtools_dir}/boot_app0.bin')
				boot_app0_copied = True
				break
			else:
				# Handle glob patterns for @src- versioned packages
				if '*' in str(boot_app0_path):
					matches = glob.glob(str(boot_app0_path))
					if matches:
						boot_app0_path = Path(matches[0])

				if boot_app0_path.exists():
					os.system(f'cp "{boot_app0_path}" {espwebtools_dir}/boot_app0.bin')
					boot_app0_copied = True
					break

		if not boot_app0_copied and not test_mode:
			print(f'⚠️  Warning: boot_app0.bin not found in Arduino ESP32 framework')

		# Generate manifest.json
		manifest = generate_espwebtools_manifest(board_name, chip_family, version, firmware_url_base)
		manifest_json = json.dumps(manifest, indent=2)

		if test_mode:
			print(f'Writing manifest.json:\n{manifest_json}')
		else:
			with open(f'{espwebtools_dir}/manifest.json', 'w') as f:
				f.write(manifest_json)
			print(f'ESP Web Tools files created in {espwebtools_dir}')

	#write our config json file
	config_str = json.dumps(config, indent=4)
	if test_mode:
		print (config_str)
	else:
		with open("releases/ota_manifest.json", "w") as text_file:
				text_file.write(config_str)

	# Update full_manifest.json with the new release information
	releases_json_path = Path("releases/full_manifest.json")

	# Load existing full_manifest.json or create new structure
	if releases_json_path.exists():
		with open(releases_json_path, "r") as f:
			releases_data = json.load(f)
	else:
		releases_data = {}

	# Add new release entries for each board
	for board_config in boards:
		board_name = board_config['name']

		# Initialize board array if it doesn't exist
		if board_name not in releases_data:
			releases_data[board_name] = []

		# Create new release entry
		new_release = {
			"version": version,
			"url": f'{firmware_url_base}{board_name}/{board_name}-{version}.bin',
			"espwebtools_manifest": f'{firmware_url_base}{board_name}-{version}-espwebtools/manifest.json',
			"changelog": changelog
		}

		# Check if this version already exists for this board
		version_exists = any(r['version'] == version for r in releases_data[board_name])

		if not version_exists:
			# Add new release at the beginning (newest first)
			releases_data[board_name].insert(0, new_release)
			print(f'Added {board_name} v{version} to releases.json')
		else:
			# Update existing release entry
			for i, r in enumerate(releases_data[board_name]):
				if r['version'] == version:
					releases_data[board_name][i] = new_release
					print(f'Updated {board_name} v{version} in releases.json')
					break

	# Write updated releases.json
	releases_json_str = json.dumps(releases_data, indent=2)
	if test_mode:
		print(f'\nWould write to releases/releases.json:\n{releases_json_str}')
	else:
		with open(releases_json_path, "w") as f:
			f.write(releases_json_str)
		print(f'Updated releases/releases.json')

	#some info to the user to finish the release
	print("Build complete.\n")

	print("Next steps:")
	print(f'1. Add the new firmware files: git add releases')
	print(f'2. Commit the new version: git commit -am "Firmware release v{version}"')
	print(f'3. Push changes to github: git push')
	print(f'4. Create a new tag: git tag -a v{version} -m "Firmware release v{version}"')
	print(f'5. Push your tags: git push origin v{version}')
	print(f'6. Create your release: gh release create v{version} --notes-file - --title "Firmware Release v{version}"\n')

	response = input("Type YES to execute these commands and complete the release: ")

	if response == "YES":
		print("\nExecuting release commands...\n")

		# 1. Add the new firmware files
		print("1. Adding firmware files...")
		if test_mode:
			print("git add releases")
		else:
			os.system("git add releases")

		# 2. Commit the new version
		print("2. Committing changes...")
		if test_mode:
			print(f'git commit -am "Firmware release v{version}"')
		else:
			os.system(f'git commit -am "Firmware release v{version}"')

		# 3. Push changes to github
		print("3. Pushing changes to GitHub...")
		if test_mode:
			print("git push")
		else:
			os.system("git push")

		# 4. Create a new tag
		print("4. Creating git tag...")
		if test_mode:
			print(f'git tag -a v{version} -m "Firmware release v{version}"')
		else:
			os.system(f'git tag -a v{version} -m "Firmware release v{version}"')

		# 5. Push your tags
		print("5. Pushing tags...")
		if test_mode:
			print(f'git push origin v{version}')
		else:
			os.system(f'git push origin v{version}')

		# 6. Create your release with changelog
		print("6. Creating GitHub release...")
		if test_mode:
			print(f'echo "{changelog}" | gh release create v{version} --notes-file - --title "Firmware Release v{version}"')
		else:
			# Pass changelog via stdin to gh release create
			import subprocess
			subprocess.run(
				['gh', 'release', 'create', f'v{version}', '--notes-file', '-', '--title', f'Firmware Release v{version}'],
				input=changelog.encode(),
				check=True
			)

		print("\n✅ Release complete!")
	else:
		print("\nRelease cancelled. You can run the commands manually if needed.")