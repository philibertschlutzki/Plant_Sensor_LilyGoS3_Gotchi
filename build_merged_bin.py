Import("env")
import os
import subprocess

def after_build(source, target, env):
    print("==> Post-build: Merging firmware binaries...")

    build_dir   = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")

    bootloader  = os.path.join(build_dir, "bootloader.bin")
    partitions  = os.path.join(build_dir, "partitions.bin")
    firmware    = os.path.join(build_dir, "firmware.bin")
    littlefs    = os.path.join(build_dir, "littlefs.bin")
    merged_bin  = os.path.join(project_dir, "LilyGo_Gotchi_Full_Merged.bin")

    # Offset aus PlatformIO-Environment lesen – wird aus der CSV-Partitionstabelle befüllt
    spiffs_start = env.get("SPIFFS_START", None)
    if spiffs_start is None:
        print("ERROR: SPIFFS_START nicht im Environment. Partitionstabelle prüfen.")
        return

    littlefs_offset = hex(spiffs_start) if isinstance(spiffs_start, int) else str(spiffs_start)
    print(f"==> LittleFS-Offset aus Partitionstabelle: {littlefs_offset}")

    # Pflicht-Binaries
    required = [
        ("0x0",    bootloader),
        ("0x8000", partitions),
        ("0x10000", firmware),
    ]

    for offset, path in required:
        if not os.path.exists(path):
            print(f"ERROR: {path} nicht gefunden – Build unvollständig.")
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
        print(f"==> LittleFS eingebunden: {littlefs}")
    else:
        print(f"WARNING: {littlefs} nicht gefunden – Web-UI fehlt im Merge!")

    print(f"==> Führe aus: {' '.join(cmd)}")
    result = subprocess.run(cmd)

    if result.returncode == 0:
        size = os.path.getsize(merged_bin)
        print(f"==> Erfolgreich: {merged_bin} ({size // 1024} KB)")
    else:
        print(f"ERROR: esptool merge_bin fehlgeschlagen (exit {result.returncode})")

env.AddPostAction("$BUILD_DIR/firmware.bin", after_build)
