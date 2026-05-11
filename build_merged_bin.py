Import("env")
import os
import subprocess
import struct

def parse_partitions_bin(partitions_file):
    """
    Parst die partitions.bin und findet den Offset der LittleFS-Partition.
    """
    try:
        with open(partitions_file, 'rb') as f:
            data = f.read()
        
        for i in range(0, len(data), 32):
            entry = data[i:i+32]
            if len(entry) < 32:
                break
            
            if entry[0] != 0xAA or entry[1] != 0x50:
                continue
            
            partition_type = entry[2]
            partition_subtype = entry[3]
            offset = struct.unpack('<I', entry[4:8])[0]
            size = struct.unpack('<I', entry[8:12])[0]
            label = entry[12:28].rstrip(b'\x00').decode('utf-8', errors='ignore')
            
            if 'littlefs' in label.lower() or 'spiffs' in label.lower():
                return offset, size
    
    except Exception as e:
        print(f"[ERROR] Fehler beim Parsen von partitions.bin: {e}")
    
    return None, None

def after_build(source, target, env):
    print("\n" + "="*60)
    print("==> POST-BUILD: Merging firmware binaries...")
    print("="*60)

    build_dir   = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")

    bootloader  = os.path.join(build_dir, "bootloader.bin")
    partitions  = os.path.join(build_dir, "partitions.bin")
    firmware    = os.path.join(build_dir, "firmware.bin")
    littlefs    = os.path.join(build_dir, "littlefs.bin")
    merged_bin  = os.path.join(project_dir, "LilyGo_Gotchi_Full_Merged.bin")

    print(f"\n[DEBUG] BUILD_DIR:    {build_dir}")
    print(f"[DEBUG] PROJECT_DIR:  {project_dir}")

    # Prüfe Pflicht-Dateien
    required = [
        ("0x0",    bootloader),
        ("0x8000", partitions),
        ("0x10000", firmware),
    ]

    print(f"\n[DEBUG] Checking required files:")
    all_exist = True
    for offset, path in required:
        exists = os.path.exists(path)
        if exists:
            size = os.path.getsize(path)
            print(f"  ✓ {os.path.basename(path)}: {size} bytes @ offset {offset}")
        else:
            print(f"  ✗ {os.path.basename(path)}: NOT FOUND")
            all_exist = False

    # WICHTIG: Prüfe littlefs.bin DETAILLIERT
    print(f"\n[DEBUG] LittleFS analysis:")
    if os.path.exists(littlefs):
        littlefs_size = os.path.getsize(littlefs)
        print(f"  ✓ littlefs.bin exists: {littlefs_size} bytes")
        
        if littlefs_size == 0:
            print(f"  ✗ ERROR: littlefs.bin ist LEER! (0 bytes)")
            print(f"  [FIX] Prüfe: Sind data/index.html, data/config.json im Repo?")
            return
        elif littlefs_size < 100000:
            print(f"  ⚠️  WARNING: littlefs.bin sehr klein ({littlefs_size} bytes)")
            print(f"  [POSSIBLE] Fehlen Dateien in data/ Verzeichnis?")
    else:
        print(f"  ✗ littlefs.bin NOT FOUND")
        return

    if not all_exist:
        print("\n[ERROR] Nicht alle erforderlichen Binaries gefunden!")
        return

    # Offset aus partitions.bin parsen
    print(f"\n[DEBUG] Parsing partitions.bin...")
    littlefs_offset_int, littlefs_size_expected = parse_partitions_bin(partitions)
    
    if littlefs_offset_int is None:
        print("[ERROR] LittleFS offset konnte nicht aus partitions.bin gelesen werden!")
        print("[ERROR] Fallback: verwende 0xc90000 (Standard für 16MB mit OTA)")
        littlefs_offset_int = 0xc90000
        littlefs_size_expected = 0x360000
    
    littlefs_offset = f"0x{littlefs_offset_int:x}"
    print(f"  Offset aus Partition: {littlefs_offset}")
    print(f"  Expected size: 0x{littlefs_size_expected:x} ({littlefs_size_expected} bytes)")

    # Kommando zusammenbauen
    cmd = [
        env.subst("$PYTHONEXE"), "-m", "esptool",
        "--chip", "esp32s3",
        "merge-bin",
        "-o", merged_bin,
        "--flash-mode", "dio",
        "--flash-freq", "80m",
        "--flash-size", "16MB",
    ]

    for offset, path in required:
        cmd.extend([offset, path])

    if os.path.exists(littlefs):
        cmd.extend([littlefs_offset, littlefs])
        print(f"\n[DEBUG] LittleFS wird eingebunden @ {littlefs_offset}")
    else:
        print(f"\n[WARNING] littlefs.bin nicht gefunden!")

    print(f"\n[DEBUG] esptool Kommando:")
    print(f"  {' '.join(cmd)}")

    print(f"\n[DEBUG] Starte esptool merge-bin...")
    try:
        result = subprocess.run(cmd, capture_output=False)
        print(f"[DEBUG] esptool exit code: {result.returncode}")
    except Exception as e:
        print(f"[ERROR] esptool Fehler: {e}")
        return

    if result.returncode == 0:
        if os.path.exists(merged_bin):
            merged_size = os.path.getsize(merged_bin)
            print(f"\n[SUCCESS] Merged binary erstellt:")
            print(f"  Datei: {merged_bin}")
            print(f"  Größe: {merged_size} bytes ({merged_size // 1048576} MB)")
            
            # Prüfe ob die Größe stimmt
            if merged_size != 0xff0000 and merged_size != 16711680:
                print(f"\n[WARNING] Unerwartete Merged-Größe!")
                print(f"  Expected: 0xff0000 (16711680 bytes)")
                print(f"  Got:      0x{merged_size:x} ({merged_size} bytes)")
        else:
            print(f"\n[ERROR] Merged binary existiert nicht!")
    else:
        print(f"\n[ERROR] esptool fehlgeschlagen (exit {result.returncode})")

    print("="*60 + "\n")

env.AddPostAction("$BUILD_DIR/firmware.bin", after_build)
