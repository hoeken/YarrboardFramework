import os
import glob
import subprocess
import datetime
Import("env")


def get_git_hash():
    """Return full commit hash, appending '-dirty' if repo has uncommitted changes."""
    try:
        # Full 40-character hash
        commit = subprocess.check_output(
            ["git", "rev-parse", "HEAD"]
        ).decode("utf-8").strip()

        # Check if working tree is dirty
        dirty = subprocess.call(
            ["git", "diff", "--quiet", "HEAD"]
        )

        if dirty != 0:
            commit += "-dirty"

        return commit
    except Exception:
        return "unknown"

def get_build_time():
    """UTC timestamp in ISO 8601 format"""
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


#print("Packaging yarrboard-client from NPM")
#env.Execute("browserify html/stub.js > html/yarrboard-client.js")

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
        gulpfile_in_current = os.path.join(project_dir, "gulpfile.mjs")

        if os.path.isfile(gulpfile_in_current):
            framework_path = project_dir
            print(f"Detected YarrboardFramework repository - using framework at: {framework_path}")
        else:
            print("ERROR: Could not find YarrboardFramework library!")
            print(f"Searched for: {framework_dir} and {framework_link}")
            print(f"DEBUG: project_dir = {project_dir}")
            print(f"DEBUG: parent_dir = {os.path.dirname(project_dir)}")
            print(f"DEBUG: grandparent_dir = {os.path.dirname(os.path.dirname(project_dir))}")
            print(f"DEBUG: Looking for gulpfile at: {gulpfile_in_current}")
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

# Set the project path - this is where project-specific assets live (html/, src/, etc.)
# For the framework examples, we need to use PROJECT_SRC_DIR (e.g., examples/platformio)
# For real projects, we want the project root directory (where platformio.ini lives)
project_path = os.getcwd()

# Check if we're building framework examples by seeing if cwd contains YarrboardFramework
if "YarrboardFramework" in project_path:
    # We're in the framework repo, use PROJECT_SRC_DIR for examples
    try:
        project_src_dir = env["PROJECT_SRC_DIR"]
        project_path = project_src_dir
        print(f"Framework example detected - using PROJECT_SRC_DIR: {project_path}")
    except KeyError:
        print(f"Framework repo but PROJECT_SRC_DIR not available, using cwd: {project_path}")
else:
    # Real project - use project root (current working directory)
    print(f"Using project root directory: {project_path}")

os.environ["YARRBOARD_PROJECT_PATH"] = project_path

# Add git version information to build flags
git_hash = get_git_hash()
build_time = get_build_time()

print(f"Git build info - Hash: {git_hash}, Build time: {build_time}")

hash_flag = "-D GIT_HASH=\\\"" + git_hash + "\\\""
time_flag = "-D BUILD_TIME=\\\"" + build_time + "\\\""

env.Append(
    BUILD_FLAGS=[hash_flag, time_flag]
)

# Check if HTML minification should be enabled
# Users can set this in their platformio.ini with: custom_enable_minify_html = yes
try:
    minify_html = env.GetProjectOption("custom_enable_minify_html", "no")
    if minify_html.lower() in ["yes", "true", "1", "on"]:
        os.environ["YARRBOARD_ENABLE_MINIFY"] = "1"
        print("HTML minification: ENABLED")
    else:
        print("HTML minification: DISABLED")
except:
    # If the option doesn't exist, minification is disabled by default
    print("HTML minification: DISABLED (default)")

print(f"Project path: {project_path}")
print("Compressing web app into header")
gulpfile_path = os.path.join(framework_path, "gulpfile.mjs")
result = env.Execute(f"gulp --gulpfile {gulpfile_path} --cwd .")
if result != 0:
    print(f"ERROR: Gulp command failed with exit code {result}")
    env.Exit(1)