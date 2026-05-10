Import("env")
import os
import sys

def after_build(source, target, env):
    print("Running post-build action: Merging firmware binaries...")

    build_dir = env.subst("$BUILD_DIR")
    # Path to individual binaries
    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    firmware = os.path.join(build_dir, "firmware.bin")
    littlefs = os.path.join(build_dir, "littlefs.bin")

    # The final merged binary
    merged_bin = os.path.join(env.subst("$PROJECT_DIR"), "LilyGo_Gotchi_Full_Merged.bin")

    offsets_and_files = [
        ("0x0", bootloader),
        ("0x8000", partitions),
        ("0x10000", firmware)
    ]

    # We must merge littlefs.bin too. But when post-build runs, littlefs.bin might not be generated yet,
    # or the user might run `pio run -t buildfs`. We just assume littlefs.bin exists or will be built,
    # but wait, post:build_merged_bin.py runs after compilation. The user might need to build the fs first.
    # Actually, we should check if littlefs exists.

    # In PlatformIO, esptool is accessible via `env.Dump()` or we can just invoke python with esptool module.
    # A cleaner approach using esptool module if it's installed, or via python -m esptool

    esptool_py = os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy") or "", "esptool.py")

    cmd = [
        env.subst("$PYTHONEXE"),
        esptool_py,
        "--chip", "esp32s3",
        "merge_bin",
        "-o", merged_bin,
        "--flash_mode", "dio",
        "--flash_freq", "80m",
        "--flash_size", "16MB"
    ]

    for offset, file in offsets_and_files:
        if os.path.exists(file):
            cmd.extend([offset, file])
        else:
            print(f"Warning: {file} not found. Build may be incomplete.")

    if os.path.exists(littlefs):
        cmd.extend(["0x310000", littlefs])
    else:
        print(f"Warning: {littlefs} not found. Please build the filesystem first (e.g. pio run -t buildfs) and run build again to include littlefs.")

    print(f"Executing: {' '.join(cmd)}")
    result = env.Execute(" ".join(cmd))

    if result == 0:
        print(f"Successfully generated {merged_bin}")
    else:
        print(f"Failed to generate {merged_bin}")

env.AddPostAction("$BUILD_DIR/firmware.bin", after_build)
