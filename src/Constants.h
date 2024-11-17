// Enhanced color scheme
#ifndef CONSTANTS_H
#define CONSTANTS_H

#define HEADER_COLOR 0x2945  // Darker blue
#define BUTTON_COLOR 0x4B0D  // Lighter blue
#define BUTTON_HIGHLIGHT 0x6B4D  // Even lighter blue for hover effect
#define TEXT_COLOR TFT_WHITE
#define BG_COLOR 0x1082  // Very dark blue
#define DANGER_COLOR 0xF800  // Red
#define SUCCESS_COLOR 0x0640  // Green

// Animation and UI constants
#define ANIMATION_SPEED 150
#define MAX_ITEMS_PER_PAGE 4
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define HEADER_HEIGHT 40
#define BUTTON_HEIGHT 50
#define BUTTON_SPACING 10
#define BUTTON_RADIUS 8

// File system constants
#define FIRMWARE_PATH "/firmware"
#define APP_PARTITION_LABEL "app0"
#define LAUNCHER_PARTITION_LABEL "launcher"

#define WDT_TIMEOUT 15  // Watchdog timeout in seconds
#define BUTTON_HOLD_TIME 3000  // Time to hold button in ms


#define CIRCLE_RADIUS 45
#define CIRCLE_SPACING 90
#define CIRCLE_Y 120
#define ANIMATION_STEPS 10
#define ANIMATION_DELAY 30

#define DRAW_BUFFER_SIZE (SCREEN_WIDTH * 150)  // Buffer for partial screen updates


#define SPRITE_WIDTH 320
#define SPRITE_HEIGHT 240
#define EASING_STEPS 40
#define TRANSITION_DURATION 100 // milliseconds

#define ANIMATION_DURATION 250  // Slightly faster animation
#define MIN_FRAME_TIME 16      // Cap at ~60fps

#define ANIMATION_BUFFER_HEIGHT 180  // Height of the animation area
#define ANIMATION_Y_START 40         // Start Y position of animation area

#define RGB565(r,g,b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define R565(color) ((color >> 11) & 0x1F)
#define G565(color) ((color >> 5) & 0x3F)
#define B565(color) (color & 0x1F)

#define ICON_SIZE 64
#define MAX_FIRMWARE_COUNT 20
#define DEFAULT_ICONS_COUNT 5
#define ICON_PATH "/icons"

#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define DARKEN_COLOR(color) (((color & 0xF7DE) >> 1) | (color & 0x0821))

#endif