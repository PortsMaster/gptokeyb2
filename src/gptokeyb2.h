/* Copyright (c) 2021-2024
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
#
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
#
* You should have received a copy of the GNU General Public
* License along with this program; if not, write to the
* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
* Boston, MA 02110-1301 USA
#
* Authored by: Kris Henriksen <krishenriksen.work@gmail.com>
#
* AnberPorts-Keyboard-Mouse
* 
* Part of the code is from from https://github.com/krishenriksen/AnberPorts/blob/master/AnberPorts-Keyboard-Mouse/main.c (mostly the fake keyboard)
* Fake Xbox code from: https://github.com/Emanem/js2xbox
* 
* Modified (badly) by: Shanti Gilbert for EmuELEC
* Modified further by: Nikolai Wuttke for EmuELEC (Added support for SDL and the SDLGameControllerdb.txt)
* Modified further by: Jacob Smith
* 
* Any help improving this code would be greatly appreciated! 
* 
* DONE: Xbox360 mode: Fix triggers so that they report from 0 to 255 like real Xbox triggers
*       Xbox360 mode: Figure out why the axis are not correctly labeled?  SDL_CONTROLLER_AXIS_RIGHTX / SDL_CONTROLLER_AXIS_RIGHTY / SDL_CONTROLLER_AXIS_TRIGGERLEFT / SDL_CONTROLLER_AXIS_TRIGGERRIGHT
*       Keyboard mode: Add a config file option to load mappings from.
*       add L2/R2 triggers
* 
*/

#ifndef __GPTOKEYB2_H__
#define __GPTOKEYB2_H__ 1

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/input.h>
#include <linux/uinput.h>

#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

#include <SDL.h>

#define GPTK2_DEBUG_ENABLED

// DEBUGGAH
#ifdef GPTK2_DEBUG_ENABLED
#define GPTK2_DEBUG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define GPTK2_DEBUG(fmt, ...) ((void)0)
#endif

// It's a lie, but it is not cake
#define XBOX_CONTROLLER_NAME "Microsoft X-Box 360 pad"

// surely this is enough. :TurtleThink: 
#define MAX_CONTROL_NAME 64

// THIS IS REDICULOUS, STOP IT.
#define CFG_STACK_MAX 16

// keyboard mods
#define MOD_SHIFT 0x01
#define MOD_CTRL  0x02
#define MOD_ALT   0x04

// BUTTON DEFS
enum
{
    GBTN_A,
    GBTN_B,
    GBTN_X,
    GBTN_Y,

    GBTN_L1,
    GBTN_L2,
    GBTN_L3,

    GBTN_R1,
    GBTN_R2,
    GBTN_R3,

    GBTN_START,
    GBTN_BACK,
    GBTN_GUIDE,

    GBTN_DPAD_UP,
    GBTN_DPAD_DOWN,
    GBTN_DPAD_LEFT,
    GBTN_DPAD_RIGHT,

    GBTN_LEFT_ANALOG_UP,
    GBTN_LEFT_ANALOG_DOWN,
    GBTN_LEFT_ANALOG_LEFT,
    GBTN_LEFT_ANALOG_RIGHT,

    GBTN_RIGHT_ANALOG_UP,
    GBTN_RIGHT_ANALOG_DOWN,
    GBTN_RIGHT_ANALOG_LEFT,
    GBTN_RIGHT_ANALOG_RIGHT,

    GBTN_MAX,

    // SPECIAL
    GBTN_DPAD,
    GBTN_LEFT_ANALOG,
    GBTN_RIGHT_ANALOG,
};


// BUTTON_STATE.state
enum
{
    BTN_UP,
    BTN_DOWN,
    BTN_HOLD, // special
};

enum
{
    ACT_NONE,
    ACT_PARENT,
    ACT_MOUSE_SLOW,
    ACT_MOUSE_MOVE,
    ACT_STATE_POP,
    // Make sure these are last, that way we can check for
    // (action >= ACT_STATE_HOLD) to see if it needs a cfg_name
    ACT_STATE_HOLD,
    ACT_STATE_PUSH,
    ACT_STATE_SET,
};

enum
{
    OPT_INT,
    OPT_STR,
};

typedef struct _gptokeyb_config gptokeyb_config;

typedef struct
{
    short keycode;
    short modifier;
    bool repeat;
    int action;

    char cfg_name[MAX_CONTROL_NAME];
    gptokeyb_config *cfg_map;
} BUTTON_MAP;


struct _gptokeyb_config
{
    gptokeyb_config *next;
    char name[MAX_CONTROL_NAME];

    bool map_check;
    BUTTON_MAP button[GBTN_MAX];
};


#define GPTK_REPEAT_DELAY 1000
#define GPTK_REPEAT_DELAY 1000

typedef struct
{
    bool pressed[GBTN_MAX];
    bool last_pressed[GBTN_MAX];

    bool pop_held[GBTN_MAX];

    bool mouse_slow;

    Uint64 held_for[GBTN_MAX];
    bool want_repeat[GBTN_MAX];
    Uint64 last_repeat[GBTN_MAX];

    int hotkey;
    bool running;

    Uint64 repeat_delay;
    Uint64 repeat_rate;
} gptokeyb_state;


// Define a struct to hold string and integer pairs
typedef struct {
    const char *str;
    int num;
} key_values;


// Define a struct to hold string and integer pairs
typedef struct {
    const char *str;
    int gbtn;
} button_match;


// Keyboard key name to codes
typedef struct {
    const char *str;
    short keycode;
    short modifier;
} keyboard_values;


extern const keyboard_values keyboard_codes[];
extern const button_match button_codes[];

extern gptokeyb_config *root_config;
extern gptokeyb_config *config_stack[];
extern gptokeyb_config *default_config;
extern gptokeyb_state current_state;
extern int gptokeyb_config_depth;

// stuff
extern int uinp_fd;
extern bool xbox360_mode;
extern bool config_mode;

// config.c
void config_init();
void config_quit();
void config_dump();
void config_finalise();

gptokeyb_config *config_find(const char *name);
gptokeyb_config *config_create(const char *name);
void config_free(gptokeyb_config *config);
int config_load(const char *file_name, bool config_only);

// keys.c
const keyboard_values *find_keyboard(const char *key);
const char *find_keycode(short keycode);
const button_match *find_button(const char *key);

// util.c -- CHATGPT
bool strendswith(const char *str, const char *suffix);
bool strcaseendswith(const char *str, const char *suffix);

bool strstartswith(const char *str, const char *prefix);
bool strcasestartswith(const char *str, const char *suffix);

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

bool process_kill();

// from og gptokeyb
void emit(int type, int code, int val);
void emitMouseMotion(int x, int y);
void emitAxisMotion(int code, int value);
void emitTextInputKey(int code, bool uppercase);
void emitKey(int code, bool is_pressed, int modifier);
void handleAnalogTrigger(bool is_triggered, bool *was_triggered, int key, int modifier);

// state.c
bool is_pressed(int btn);
bool was_pressed(int btn);
bool was_released(int btn);

void state_init();
void state_update();

void push_state(gptokeyb_config *);
void set_state(gptokeyb_config *);
void pop_state();

// event.c
bool handleInputEvent(const SDL_Event *event);

// keyboard.c
void setupFakeKeyboardMouseDevice(struct uinput_user_dev *device, int fd);
void handleEventBtnFakeKeyboardMouseDevice(const SDL_Event *event, bool is_pressed);
void handleEventAxisFakeKeyboardMouseDevice(const SDL_Event *event);

// xbox360.c
void setupFakeXbox360Device(struct uinput_user_dev *device, int fd);
void handleEventBtnFakeXbox360Device(const SDL_Event *event, bool is_pressed);
void handleEventAxisFakeXbox360Device(const SDL_Event *event);


#endif
