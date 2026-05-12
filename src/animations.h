#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <Arduino.h>

#define AVATAR_W 64
#define AVATAR_H 64
#define ICON_W 16
#define ICON_H 16

// p0
extern const uint16_t p0_happy[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p0_sad[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p0_freeze[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p0_sleep[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p0_surprised[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p0_closed[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p0_offline[AVATAR_W * AVATAR_H] PROGMEM;

// p1
extern const uint16_t p1_happy[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p1_sad[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p1_freeze[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p1_sleep[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p1_surprised[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p1_closed[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p1_offline[AVATAR_W * AVATAR_H] PROGMEM;

// p2
extern const uint16_t p2_happy[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p2_sad[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p2_freeze[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p2_sleep[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p2_surprised[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p2_closed[AVATAR_W * AVATAR_H] PROGMEM;
extern const uint16_t p2_offline[AVATAR_W * AVATAR_H] PROGMEM;

// Icons
extern const uint16_t icon_drop_full[ICON_W * ICON_H] PROGMEM;
extern const uint16_t icon_drop_empty[ICON_W * ICON_H] PROGMEM;
extern const uint16_t icon_drop_gray[ICON_W * ICON_H] PROGMEM;
extern const uint16_t icon_leaf_full[ICON_W * ICON_H] PROGMEM;
extern const uint16_t icon_leaf_empty[ICON_W * ICON_H] PROGMEM;
extern const uint16_t icon_leaf_gray[ICON_W * ICON_H] PROGMEM;
extern const uint16_t icon_sun_full[ICON_W * ICON_H] PROGMEM;
extern const uint16_t icon_sun_empty[ICON_W * ICON_H] PROGMEM;
extern const uint16_t icon_sun_gray[ICON_W * ICON_H] PROGMEM;
extern const uint16_t icon_temp_blue[ICON_W * ICON_H] PROGMEM;
extern const uint16_t icon_temp_red[ICON_W * ICON_H] PROGMEM;
extern const uint16_t icon_batt_warn[ICON_W * ICON_H] PROGMEM;

// Overlays
extern const uint16_t overlay_sunglasses[AVATAR_W * AVATAR_H] PROGMEM;

#endif
