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
#include <inttypes.h>

#include <linux/input.h>
#include <linux/uinput.h>

#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

#include <SDL.h>

#include "interpose.h"

#define GPTK2_VERSION "2.00.03"

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
#define MAX_CONTROLLERS 64
#define MAX_CONTROL_NAME 64

// THIS IS REDICULOUS, STOP IT.
#define CFG_STACK_MAX 16

// This should be tested to find a better value.
#define DEFAULT_MOUSE_WHEEL_AMOUNT 1

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
    GBTN_NONE = -1,
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
{   // Action mode
    ACT_NONE,
    ACT_PARENT,
    ACT_SPECIAL,
    ACT_STATE_POP,
    // Make sure these are last, that way we can check for
    // (action >= ACT_STATE_HOLD) to see if it needs a cfg_name
    ACT_STATE_HOLD,
    ACT_STATE_PUSH,
    ACT_STATE_SET,
};


enum
{   // Special mode
    SPC_NONE,
    SPC_MOUSE_SLOW,

    SPC_ADD_LETTER,
    SPC_REM_LETTER,

    SPC_NEXT_LETTER,
    SPC_PREV_LETTER,

    SPC_NEXT_WORD,
    SPC_PREV_WORD,

    SPC_UPPER_CASE,
    SPC_LOWER_CASE,
    SPC_TOGGLE_CASE,

    SPC_ACCEPT_INPUT,
    SPC_CANCEL_INPUT,
};

enum
{   // Exclusive mode
    EXL_FALSE,
    EXL_TRUE,

    EXL_PARENT,
};

enum
{   // Overlay mode
    OVL_NONE,
    OVL_PARENT,
    OVL_CLEAR,
};

// simple tokenizer
typedef struct _token_ctx token_ctx;


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
    int special;

    const char *cfg_name;
    gptokeyb_config *cfg_map;
} gptokeyb_button;


struct _gptokeyb_config
{
    gptokeyb_config *next;
    const char *name;

    const char *charset;
    const char *wordset;

    const char *input_return_name;
    gptokeyb_config *input_return_map;

    int overlay_mode; // one of OVL_NONE / PARENT / CLEAR / NAMED
    int exclusive_mode;

    // one of MOUSE_MOVEMENT_PARENT / OFF / ON
    int left_analog_as_mouse;
    int right_analog_as_mouse;
    int dpad_as_mouse;

    // same as above but using absolute positional values
    int left_analog_as_absolute_mouse;
    int right_analog_as_absolute_mouse;

    // Amount to scroll the wheel, 0 means parent amount or default.
    Uint32 mouse_wheel_amount;

    bool map_check;
    gptokeyb_button button[GBTN_MAX];
};


typedef struct
{
    Uint32 pressed;
    Uint32 last_pressed;
    Uint32 pop_held;

    Uint32 mouse_slow;
    Uint32 mouse_move;

    Uint32 in_repeat;
    Uint32 held_since[GBTN_MAX];
    Uint32 next_repeat[GBTN_MAX];

    const gptokeyb_button *button_held[GBTN_MAX];

    int current_left_analog_x;
    int current_left_analog_y;

    int current_right_analog_x;
    int current_right_analog_y;

    int current_l2;
    int current_r2;

    int mouse_relative_x;
    int mouse_relative_y;

    bool absolute_invert_x;
    bool absolute_invert_y;
    int absolute_center_x;
    int absolute_center_y;
    int absolute_step;
    int absolute_deadzone;
    int absolute_rotate;
    int absolute_screen_width;
    int absolute_screen_height;
    int mouse_absolute_x;
    int mouse_absolute_y;

    int dpad_mouse_step;
    int mouse_slow_scale;
    bool dpad_mouse_normalize;

    int deadzone_mode;
    int deadzone_scale;

    int deadzone_x;
    int deadzone_y;
    int deadzone_triggers;

    int hotkey_gbtn;
    bool running;

    Uint64 mouse_delay;
    Uint64 repeat_delay;
    Uint64 repeat_rate;
} gptokeyb_state;


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


// input stuff
typedef struct _char_set {
    struct _char_set *next;
    const char *name;
    const char *characters;
    size_t characters_len;
    bool builtin;
} char_set;


typedef struct _word_set {
    struct _word_set *next;
    const char *name;
    const char **words;
    size_t words_len;
    size_t words_alloc;
} word_set;


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
extern bool current_mouse_wheel_amount;
extern bool current_left_analog_as_absolute_mouse;
extern bool current_right_analog_as_absolute_mouse;

// fds for emulated devices
extern int xbox_uinp_fd; // fake xbox controller
extern int kb_uinp_fd;   // fake relative mouse and keyboard
extern int abs_uinp_fd;  // fake absolute position mouse

// stuff
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
void *gptk_malloc(size_t);
void *gptk_realloc(void *, size_t, size_t);

token_ctx *tokens_create(const char *input_text, char separator);
void tokens_free(token_ctx *token_state);
const char *tokens_next(token_ctx *token_state);
const char *tokens_prev(token_ctx *token_state);
const char *tokens_curr(token_ctx *token_state);
const char *tokens_rest(token_ctx *token_state);

char *tabulate_text(const char *text);

bool strendswith(const char *str, const char *suffix);
bool strcaseendswith(const char *str, const char *suffix);

bool strstartswith(const char *str, const char *prefix);
bool strcasestartswith(const char *str, const char *suffix);

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

bool process_kill();

void string_init();
void string_quit();
const char *string_register(const char *string);

// from og gptokeyb
void emit(int fd, int type, int code, int val);
void emitRelativeMouseMotion(int x, int y);
void emitAbsoluteMouseMotion(int x, int y);
void emitMouseWheel(int wheel);
void emitAxisMotion(int code, int value);
void emitTextInputKey(int code, bool uppercase);
void emitKey(int fd, int code, bool is_pressed, int modifier);
void handleAnalogTrigger(bool is_triggered, bool *was_triggered, int key, int modifier);

// input.c
const char_set *find_char_set(const char *name);
const word_set *find_word_set(const char *name);

void input_init();
void input_quit();

void register_char_set(const char *name, const char *characters);
void register_word_set(const char *name, const char *word);

void input_load_char_set(const char *name);
void input_load_word_set(const char *name);
void input_stop();

void input_accept();
void input_cancel();

void input_disable();

bool input_active();

void input_set_state(const char *buff, size_t buff_len);
void input_clear_state();

void dump_word_sets();
void dump_char_sets();

void input_add_letter();
void input_rem_letter();
void input_next_letter(int amount);
void input_prev_letter(int amount);
void input_next_word(int amount);
void input_prev_word(int amount);

void input_upper_case();
void input_lower_case();
void input_toggle_case();

// state.c
bool is_pressed(int btn);
bool was_pressed(int btn);
bool was_released(int btn);

void update_button(int btn, bool pressed);

void state_init();
void state_quit();
void state_update();
gptokeyb_config *state_active();

void push_state(gptokeyb_config *);
void set_state(gptokeyb_config *);
void pop_state();
void state_change_update();

void controllers_enable_exclusive();
void controllers_disable_exclusive();

void controller_add_fd(Sint32 which, int fd);
void controller_remove_fd(Sint32 which);

// event.c
bool isExistingController(SDL_JoystickID id);
void recordExistingControllers();
void handleInputEvent(const SDL_Event *event);

// keyboard.c
void setupFakeKeyboardMouseDevice();
void setupFakeAbsoluteMouseDevice();
void handleEventBtnFakeKeyboardMouseDevice(const SDL_Event *event, bool is_pressed);
void handleEventAxisFakeKeyboardMouseDevice(const SDL_Event *event);

// xbox360.c
void setupFakeXbox360Device();
void handleEventBtnFakeXbox360Device(const SDL_Event *event, bool is_pressed);
void handleEventAxisFakeXbox360Device(const SDL_Event *event);


#endif
