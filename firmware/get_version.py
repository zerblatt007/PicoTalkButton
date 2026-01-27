Import("env")
import subprocess

def get_firmware_version():
    try:
        # Get git describe output (e.g., "v1.0.0-5-g1234abc" or "v1.0.0")
        version = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            stderr=subprocess.DEVNULL
        ).decode('utf-8').strip()
        return version
    except:
        # Fallback if git fails (not in repo, no tags, etc.)
        try:
            # Just get short commit hash
            version = subprocess.check_output(
                ["git", "rev-parse", "--short", "HEAD"],
                stderr=subprocess.DEVNULL
            ).decode('utf-8').strip()
            return f"git-{version}"
        except:
            return "unknown"

version = get_firmware_version()
print(f"Building firmware version: {version}")

# Add version as a build flag
env.Append(CPPDEFINES=[
    ("FIRMWARE_VERSION", f'\\"{version}\\"')
])
