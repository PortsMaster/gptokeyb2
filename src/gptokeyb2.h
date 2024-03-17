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

#ifndef SDL_DEFAULT_REPEAT_DELAY
#define SDL_DEFAULT_REPEAT_DELAY 500
#endif

#ifndef SDL_DEFAULT_REPEAT_INTERVAL
#define SDL_DEFAULT_REPEAT_INTERVAL 60
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


// Deadzone modes
enum
{
    DZ_DEFAULT,
    DZ_AXIAL,
    DZ_RADIAL,
    DZ_SCALED_RADIAL,
    DZ_SLOPED_AXIAL,
    DZ_SLOPED_SCALED_AXIAL,
    DZ_HYBRID,
};


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

#define GBTN_IS_DPAD(gbtn) \
    ((gbtn == GBTN_DPAD_UP)   || \
     (gbtn == GBTN_DPAD_DOWN) || \
     (gbtn == GBTN_DPAD_LEFT) || \
     (gbtn == GBTN_DPAD_RIGHT))

#define GBTN_IS_LEFT_ANALOG(gbtn) \
    ((gbtn == GBTN_LEFT_ANALOG_UP)   || \
     (gbtn == GBTN_LEFT_ANALOG_DOWN) || \
     (gbtn == GBTN_LEFT_ANALOG_LEFT) || \
     (gbtn == GBTN_LEFT_ANALOG_RIGHT))

#define GBTN_IS_RIGHT_ANALOG(gbtn) \
    ((gbtn == GBTN_RIGHT_ANALOG_UP)   || \
     (gbtn == GBTN_RIGHT_ANALOG_DOWN) || \
     (gbtn == GBTN_RIGHT_ANALOG_LEFT) || \
     (gbtn == GBTN_RIGHT_ANALOG_RIGHT))

enum
{
    ACT_NONE,
    ACT_PARENT,
    ACT_MOUSE_SLOW,
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

#define MOUSE_MOVEMENT_PARENT -1
#define MOUSE_MOVEMENT_OFF 0
#define MOUSE_MOVEMENT_ON 1

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

    // one of MOUSE_MOVEMENT_PARENT / OFF / ON
    int left_analog_as_mouse;
    int right_analog_as_mouse;
    int dpad_as_mouse;

    bool map_check;
    BUTTON_MAP button[GBTN_MAX];
};



typedef struct
{
    bool pressed[GBTN_MAX];
    bool last_pressed[GBTN_MAX];

    bool pop_held[GBTN_MAX];

    int current_left_analog_x;
    int current_left_analog_y;

    int current_right_analog_x;
    int current_right_analog_y;

    int current_l2;
    int current_r2;

    int mouse_x;
    int mouse_y;

    Uint32 mouse_slow;
    Uint32 mouse_move;

    int dpad_mouse_step;
    int mouse_slow_scale;
    bool dpad_mouse_normalize;

    int deadzone_mode;
    int deadzone_scale;

    int deadzone_x;
    int deadzone_y;
    int deadzone_triggers;

    bool in_repeat[GBTN_MAX];
    Uint32 held_since[GBTN_MAX];
    Uint32 next_repeat[GBTN_MAX];

    int hotkey_gbtn;
    bool running;

    Uint64 repeat_delay;
    Uint64 repeat_rate;
} gptokeyb_state;


// Define a struct to hold string and integer pairs
typedef struct {
    const char *str;
    int type;
    const char *default_str;
    int default_value;
} option_values;


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

// Basic vector 2d class for better analog deadzone code
typedef struct
{
    float x;
    float y;
} vector2d;

// some stuff
extern const keyboard_values keyboard_codes[];
extern const button_match button_codes[];

extern const char *gbtn_names[];
extern const char *act_names[];
extern char default_control_name[];

extern gptokeyb_config *root_config;
extern gptokeyb_config *config_stack[];
extern gptokeyb_config *default_config;
extern gptokeyb_config *config_temp_stack[];
extern int config_temp_stack_order[];
extern int config_temp_stack_order_id;

extern gptokeyb_state current_state;
extern int gptokeyb_config_depth;
// these get filled out as the state changes
extern bool current_dpad_as_mouse;
extern bool current_left_analog_as_mouse;
extern bool current_right_analog_as_mouse;

// stuff
extern int uinp_fd;
extern bool xbox360_mode;
extern bool config_mode;

extern bool want_pc_quit;
extern bool want_kill;
extern bool want_sudo;
extern char kill_process_name[];

extern char game_prefix[];

// config.c
void config_init();
void config_quit();
void config_dump();
void config_finalise();

void config_overlay_clear(gptokeyb_config *current);
gptokeyb_config *config_find(const char *name);
gptokeyb_config *config_create(const char *name);
void config_free(gptokeyb_config *config);
int config_load(const char *file_name, bool config_only);

// analog.c
void vector2d_clear(vector2d *vec2d);
void vector2d_set_vector2d(vector2d *vec2d, const vector2d *other);
void vector2d_set_float(vector2d *vec2d, float v);
void vector2d_set_float2(vector2d *vec2d, float x, float y);
void vector2d_normalize(vector2d *vec2d);
float vector2d_magnitude(const vector2d *vec2d);

int deadzone_get_mode(const char *str);
const char *deadzone_mode_str(int mode);
void deadzone_trigger_calc(int *analog, int analog_in);
void deadzone_mouse_calc(int *x, int *y, int in_x, int in_y);

// keys.c
const keyboard_values *find_keyboard(const char *key);
const char *find_keycode(short keycode);
const button_match *find_button(const char *key);
void set_hotkey(int gbtn);

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

void update_button(int btn, bool pressed);

void state_init();
void state_update();
gptokeyb_config *state_active();

void push_state(gptokeyb_config *);
void set_state(gptokeyb_config *);
void pop_state();
void state_change_update();

// event.c
void handleInputEvent(const SDL_Event *event);

// keyboard.c
void setupFakeKeyboardMouseDevice(struct uinput_user_dev *device, int fd);
void handleEventBtnFakeKeyboardMouseDevice(const SDL_Event *event, bool is_pressed);
void handleEventAxisFakeKeyboardMouseDevice(const SDL_Event *event);

// xbox360.c
void setupFakeXbox360Device(struct uinput_user_dev *device, int fd);
void handleEventBtnFakeXbox360Device(const SDL_Event *event, bool is_pressed);
void handleEventAxisFakeXbox360Device(const SDL_Event *event);


#endif
