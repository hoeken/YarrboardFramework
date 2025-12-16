Import("env")
import subprocess
import datetime

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
    return datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")

git_hash = get_git_hash()
build_time = get_build_time()

print(f"Git build info - Hash: {git_hash}, Build time: {build_time}")

hash_flag = "-D GIT_HASH=\\\"" + git_hash + "\\\""
time_flag = "-D BUILD_TIME=\\\"" + build_time + "\\\""

env.Append(
    BUILD_FLAGS=[hash_flag, time_flag]
)