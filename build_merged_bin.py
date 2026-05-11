Import("env")
import os
import subprocess
import sys

def after_build(source, target, env):
    print("\n" + "="*60)
    print("==> POST-BUILD: Merging firmware binaries...")
    print("="*60)

    build_dir   = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")

    print(f"\n[DEBUG] BUILD_DIR:    {build_dir}")
    print(f"[DEBUG] PROJECT_DIR:  {project_dir}")
    print(f"[DEBUG] PYTHONEXE:    {env.subst('$PYTHONEXE')}")

    bootloader  = os.path.join(build_dir, "bootloader.bin")
    partitions  = os.path.join(build_dir, "partitions.bin")
    firmware    = os.path.join(build_dir, "firmware.bin")
    littlefs    = os.path.join(build_dir, "littlefs.bin")
    merged_bin  = os.path.join(project_dir, "LilyGo_Gotchi_Full_Merged.bin")

    print(f"\n[DEBUG] Expected files:")
    print(f"  bootloader: {bootloader}")
    print(f"  partitions: {partitions}")
    print(f"  firmware:   {firmware}")
    print(f"  littlefs:   {littlefs}")
    print(f"  merged_bin: {merged_bin}")

    print(f"\n[DEBUG] File existence check:")
    print(f"  bootloader exists: {os.path.exists(bootloader)}")
    print(f"  partitions exists: {os.path.exists(partitions)}")
    print(f"  firmware exists:   {os.path.exists(firmware)}")
    print(f"  littlefs exists:   {os.path.exists(littlefs)}")

    print(f"\n[DEBUG] BUILD_DIR contents:")
    if os.path.isdir(build_dir):
        for item in os.listdir(build_dir):
            full_path = os.path.join(build_dir, item)
            size = os.path.getsize(full_path) if os.path.isfile(full_path) else "DIR"
            print(f"    {item}: {size}")
    else:
        print(f"    BUILD_DIR existiert nicht!")

    # SPIFFS_START auslesen
    spiffs_start = env.get("SPIFFS_START", None)
    print(f"\n[DEBUG] Environment variables:")
    print(f"  SPIFFS_START: {spiffs_start}")

    if spiffs_start is None:
        print("\n[ERROR] SPIFFS_START nicht im Environment!")
        print("[ERROR] Mögliche Ursachen:")
        print("  1. partitions_16MB_ota.csv existiert nicht oder ist fehlerhaft")
        print("  2. board_build.partitions = partitions_16MB_ota.csv nicht in platformio.ini")
        print("  3. PlatformIO hat Partitionstabelle nicht geparsed")
        return

    littlefs_offset = hex(spiffs_start) if isinstance(spiffs_start, int) else str(spiffs_start)
    print(f"\n[DEBUG] LittleFS offset: {littlefs_offset}")

    # Prüfe Pflicht-Dateien
    required = [
        ("0x0",    bootloader),
        ("0x8000", partitions),
        ("0x10000", firmware),
    ]

    print(f"\n[DEBUG] Checking required files...")
    all_exist = True
    for offset, path in required:
        exists = os.path.exists(path)
        if exists:
            size = os.path.getsize(path)
            print(f"  ✓ {os.path.basename(path)}: {size} bytes @ offset {offset}")
        else:
            print(f"  ✗ {os.path.basename(path)}: NOT FOUND @ offset {offset}")
            all_exist = False

    if not all_exist:
        print("\n[ERROR] Nicht alle erforderlichen Binaries gefunden!")
        return

    # Kommando zusammenbauen
    cmd = [
        env.subst("$PYTHONEXE"), "-m", "esptool",
        "--chip", "esp32s3",
        "merge_bin",
        "-o", merged_bin,
        "--flash_mode", "dio",
        "--flash_freq", "80m",
        "--flash_size", "16MB",
    ]

    for offset, path in required:
        cmd.extend([offset, path])

    if os.path.exists(littlefs):
        cmd.extend([littlefs_offset, littlefs])
        print(f"\n[DEBUG] LittleFS wird eingebunden @ {littlefs_offset}")
    else:
        print(f"\n[WARNING] {littlefs} nicht gefunden – Web-UI wird fehlen!")

    print(f"\n[DEBUG] esptool Kommando:")
    print(f"  {' '.join(cmd)}")

    print(f"\n[DEBUG] Starte esptool merge_bin...")
    try:
        result = subprocess.run(cmd, capture_output=False)
        print(f"[DEBUG] esptool exit code: {result.returncode}")
    except Exception as e:
        print(f"[ERROR] esptool Fehler: {e}")
        return

    if result.returncode == 0:
        if os.path.exists(merged_bin):
            size = os.path.getsize(merged_bin)
            print(f"\n[SUCCESS] Merged binary erstellt:")
            print(f"  Datei: {merged_bin}")
            print(f"  Größe: {size} bytes ({size // 1024} KB / {size // 1048576} MB)")
        else:
            print(f"\n[ERROR] Merged binary existiert nicht, obwohl esptool exit=0!")
    else:
        print(f"\n[ERROR] esptool fehlgeschlagen (exit {result.returncode})")

    print("="*60 + "\n")

env.AddPostAction("$BUILD_DIR/firmware.bin", after_build)
