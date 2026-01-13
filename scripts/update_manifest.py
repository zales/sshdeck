import json
import sys
import os
from datetime import datetime
import urllib.request
import ssl

def update_manifest(repo, tag, output_file="web_flasher/firmware.json", live_url="https://zales.github.io/sshdeck/firmware.json"):
    # Create default structure
    manifest = {
        "name": "SshDeck Firmware",
        "latest": tag,
        "versions": []
    }
    
    # Try to fetch existing manifest
    try:
        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        with urllib.request.urlopen(live_url, context=ctx) as response:
            if response.status == 200:
                data = json.loads(response.read().decode())
                # specific validation could go here
                if "versions" in data:
                    manifest = data
    except Exception as e:
        print(f"Could not fetch existing manifest or first run: {e}")

    # Prepare new entry
    # Note: URL is pointing to GitHub Releases. valid after release is created.
    new_entry = {
        "version": tag,
        "date": datetime.now().strftime("%Y-%m-%d"),
        "url": f"https://github.com/{repo}/releases/download/{tag}/firmware.bin"
    }

    # Update manifest list (prevent duplicates)
    # Remove existing entry with same version if exists
    manifest["versions"] = [v for v in manifest["versions"] if v["version"] != tag]
    
    # Prepend new version
    manifest["versions"].insert(0, new_entry)
    manifest["latest"] = tag

    # Write to file
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    with open(output_file, "w") as f:
        json.dump(manifest, f, indent=4)
        print(f"Manifest written to {output_file}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python update_manifest.py <repo> <tag>")
        sys.exit(1)
    
    update_manifest(sys.argv[1], sys.argv[2])
