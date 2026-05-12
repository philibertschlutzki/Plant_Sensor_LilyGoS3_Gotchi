import sys

plants = ["p0", "p1", "p2"]
emotions = ["happy", "sad", "freeze", "sleep", "surprised", "closed", "offline"]
icons = [
    "icon_drop_full", "icon_drop_empty", "icon_drop_gray",
    "icon_leaf_full", "icon_leaf_empty", "icon_leaf_gray",
    "icon_sun_full", "icon_sun_empty", "icon_sun_gray",
    "icon_temp_blue", "icon_temp_red",
    "icon_batt_warn"
]

header = """#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <Arduino.h>

#define AVATAR_W 64
#define AVATAR_H 64
#define ICON_W 16
#define ICON_H 16

"""

for p in plants:
    header += f"// {p}\n"
    for e in emotions:
        header += f"extern const uint16_t {p}_{e}[AVATAR_W * AVATAR_H] PROGMEM;\n"
    header += "\n"

header += "// Icons\n"
for icon in icons:
    header += f"extern const uint16_t {icon}[ICON_W * ICON_H] PROGMEM;\n"

header += "\n// Overlays\n"
header += "extern const uint16_t overlay_sunglasses[AVATAR_W * AVATAR_H] PROGMEM;\n\n"
header += "#endif\n"

with open("src/animations.h", "w") as f:
    f.write(header)

cpp = """#include "animations.h"

"""

for p in plants:
    cpp += f"// {p}\n"
    for e in emotions:
        cpp += f"const uint16_t {p}_{e}[AVATAR_W * AVATAR_H] PROGMEM = {{0}};\n"
    cpp += "\n"

cpp += "// Icons\n"
for icon in icons:
    cpp += f"const uint16_t {icon}[ICON_W * ICON_H] PROGMEM = {{0}};\n"

cpp += "\n// Overlays\n"
cpp += "const uint16_t overlay_sunglasses[AVATAR_W * AVATAR_H] PROGMEM = {0};\n"

with open("src/animations.cpp", "w") as f:
    f.write(cpp)

print("Generated animations.h and animations.cpp")
