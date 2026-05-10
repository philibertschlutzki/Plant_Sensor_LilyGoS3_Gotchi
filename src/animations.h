#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <Arduino.h>

#define GHOST_WIDTH 64
#define GHOST_HEIGHT 64

extern const uint16_t ghost_idle_frame1[GHOST_WIDTH * GHOST_HEIGHT] PROGMEM;
extern const uint16_t ghost_idle_frame2[GHOST_WIDTH * GHOST_HEIGHT] PROGMEM;

extern const uint16_t ghost_happy_frame1[GHOST_WIDTH * GHOST_HEIGHT] PROGMEM;
extern const uint16_t ghost_happy_frame2[GHOST_WIDTH * GHOST_HEIGHT] PROGMEM;

extern const uint16_t ghost_sad_frame1[GHOST_WIDTH * GHOST_HEIGHT] PROGMEM;
extern const uint16_t ghost_sad_frame2[GHOST_WIDTH * GHOST_HEIGHT] PROGMEM;

extern const uint16_t ghost_sweat_frame1[GHOST_WIDTH * GHOST_HEIGHT] PROGMEM;
extern const uint16_t ghost_sweat_frame2[GHOST_WIDTH * GHOST_HEIGHT] PROGMEM;

extern const uint16_t ghost_freeze_frame1[GHOST_WIDTH * GHOST_HEIGHT] PROGMEM;
extern const uint16_t ghost_freeze_frame2[GHOST_WIDTH * GHOST_HEIGHT] PROGMEM;

#endif
