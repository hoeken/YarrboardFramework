import os
import glob
Import("env")


#print("Packaging yarrboard-client from NPM")
#env.Execute("browserify html/stub.js > html/yarrboard-client.js")

# Ensure the output directory exists
gulp_folder = "src/gulp"
if not os.path.exists(gulp_folder):
    print(f"Creating missing directory: {gulp_folder}")
    os.makedirs(gulp_folder)

# Find the YarrboardFramework library path
# PlatformIO installs dependencies in .pio/libdeps/[environment_name]/
framework_path = None

# Get the current build environment name
try:
    current_env = env["PIOENV"]
    print(f"Current build environment: {current_env}")

    # Look for YarrboardFramework in the current environment's libdeps directory
    # PlatformIO may use either a directory or a .pio-link file for symlinked libraries
    libdeps_dir = os.path.join(".pio", "libdeps", current_env)
    framework_dir = os.path.join(libdeps_dir, "YarrboardFramework")
    framework_link = os.path.join(libdeps_dir, "YarrboardFramework.pio-link")

    if os.path.isdir(framework_dir):
        framework_path = framework_dir
        print(f"Found YarrboardFramework directory at: {framework_path}")
    elif os.path.isfile(framework_link):
        # Read the .pio-link file to get the actual path
        import json
        with open(framework_link, 'r') as f:
            link_data = json.load(f)
            # The uri field contains "symlink://<relative_path>"
            uri = link_data.get('spec', {}).get('uri', '')
            if uri.startswith('symlink://'):
                rel_path = uri.replace('symlink://', '')
                # Resolve relative to the project root (cwd in the link file)
                framework_path = os.path.abspath(os.path.join(link_data.get('cwd', '.'), rel_path))
                print(f"Found YarrboardFramework via .pio-link at: {framework_path}")
            else:
                print(f"ERROR: Unexpected URI format in .pio-link file: {uri}")
                env.Exit(1)
    else:
        # Final fallback: Check if we're running inside the YarrboardFramework repository itself
        # This happens when building examples within the framework repo
        project_dir = os.getcwd()
        parent_dir = os.path.dirname(project_dir)
        grandparent_dir = os.path.dirname(parent_dir)

        print(f"DEBUG: project_dir = {project_dir}")
        print(f"DEBUG: parent_dir = {parent_dir}")
        print(f"DEBUG: grandparent_dir = {grandparent_dir}")
        print(f"DEBUG: os.path.basename(parent_dir) = {os.path.basename(parent_dir)}")
        print(f"DEBUG: os.path.basename(project_dir) = {os.path.basename(project_dir)}")

        # Check if we're in the YarrboardFramework repo root itself
        # (scripts/gulpfile.mjs exists in current directory)
        gulpfile_in_current = os.path.join(project_dir, "scripts", "gulpfile.mjs")
        print(f"DEBUG: Looking for gulpfile at: {gulpfile_in_current}")
        print(f"DEBUG: Gulpfile exists? {os.path.isfile(gulpfile_in_current)}")

        if os.path.isfile(gulpfile_in_current):
            framework_path = project_dir
            print(f"Detected YarrboardFramework repository - using framework at: {framework_path}")
        else:
            print("ERROR: Could not find YarrboardFramework library!")
            print(f"Searched for: {framework_dir} and {framework_link}")
            print("Make sure the library is installed via platformio.ini")
            env.Exit(1)
except KeyError:
    print("Warning: Could not determine current environment, falling back to search")
    # Fallback: search all environments
    libdeps_pattern = os.path.join(".pio", "libdeps", "*", "YarrboardFramework")
    matches = glob.glob(libdeps_pattern)

    if matches:
        framework_path = matches[0]
        print(f"Found YarrboardFramework at: {framework_path}")
    else:
        print("ERROR: Could not find YarrboardFramework library!")
        print("Make sure the library is installed via platformio.ini")
        env.Exit(1)

if not framework_path:
    print("ERROR: Framework path not set!")
    env.Exit(1)

# Set the environment variable for the gulpfile
os.environ["YARRBOARD_FRAMEWORK_PATH"] = framework_path

print("Compressing web app into header")
result = env.Execute(f"gulp --gulpfile {framework_path}/scripts/gulpfile.mjs --cwd .")
if result != 0:
    print(f"ERROR: Gulp command failed with exit code {result}")
    env.Exit(1)