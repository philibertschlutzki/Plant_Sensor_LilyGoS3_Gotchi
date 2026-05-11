Import("env")
import os
import subprocess
import struct

def parse_partitions_bin(partitions_file):
    """
    Parst die partitions.bin und findet den Offset der LittleFS-Partition.
    
    Partition table entry format (32 bytes):
    - magic: 0xAA, 0x50 (2 bytes)
    - type: 1 byte
    - subtype: 1 byte
    - offset: 4 bytes (little-endian)
    - size: 4 bytes (little-endian)
    - label: 16 bytes (null-terminated string)
    - flags: 4 bytes
    """
    try:
        with open(partitions_file, 'rb') as f:
            data = f.read()
        
        # Partition entries are 32 bytes each
        for i in range(0, len(data), 32):
            entry = data[i:i+32]
            if len(entry) < 32:
                break
            
            # Check magic bytes
            if entry[0] != 0xAA or entry[1] != 0x50:
                continue
            
            partition_type = entry[2]
            partition_subtype = entry[3]
            offset = struct.unpack('<I', entry[4:8])[0]
            size = struct.unpack('<I', entry[8:12])[0]
            label = entry[12:28].rstrip(b'\x00').decode('utf-8', errors='ignore')
            
            # Suche nach "littlefs" oder "spiffs" Partition
            if 'littlefs' in label.lower() or 'spiffs' in label.lower():
                return offset
    
    except Exception as e:
        print(f"[ERROR] Fehler beim Parsen von partitions.bin: {e}")
    
    return None

def after_build(source, target, env):
    print("\n" + "="*60)
    print("==> POST-BUILD: Merging firmware binaries...")
    print("="*60)

    build_dir   = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")

    print(f"\n[DEBUG] BUILD_DIR:    {build_dir}")
    print(f"[DEBUG] PROJECT_DIR:  {project_dir}")

    bootloader  = os.path.join(build_dir, "bootloader.bin")
    partitions  = os.path.join(build_dir, "partitions.bin")
    firmware    = os.path.join(build_dir, "firmware.bin")
    littlefs    = os.path.join(build_dir, "littlefs.bin")
    merged_bin  = os.path.join(project_dir, "LilyGo_Gotchi_Full_Merged.bin")

    print(f"\n[DEBUG] File existence check:")
    print(f"  bootloader exists: {os.path.exists(bootloader)}")
    print(f"  partitions exists: {os.path.exists(partitions)}")
    print(f"  firmware exists:   {os.path.exists(firmware)}")
    print(f"  littlefs exists:   {os.path.exists(littlefs)}")

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

    # Offset aus partitions.bin parsen
    print(f"\n[DEBUG] Parsing partitions.bin für LittleFS offset...")
    littlefs_offset_int = parse_partitions_bin(partitions)
    
    if littlefs_offset_int is None:
        print("[ERROR] LittleFS offset konnte nicht aus partitions.bin gelesen werden!")
        print("[ERROR] Fallback: verwende 0xc90000 (Standard für 16MB mit OTA)")
        littlefs_offset_int = 0xc90000
    
    littlefs_offset = f"0x{littlefs_offset_int:x}"
    print(f"[DEBUG] LittleFS offset: {littlefs_offset}")

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
