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

#include "gptokeyb2.h"

const keyboard_values keyboard_codes[] = {
    {"up",          KEY_UP,          0},
    {"down",        KEY_DOWN,        0},
    {"left",        KEY_LEFT,        0},
    {"right",       KEY_RIGHT,       0},
    {"mouse_left",  BTN_LEFT,        0},
    {"mouse_right", BTN_RIGHT,       0},
    {"space",       KEY_SPACE,       0},
    {" ",           KEY_SPACE,       0},
    {"esc",         KEY_ESC,         0},
    {"end",         KEY_END,         0},
    {"home",        KEY_HOME,        0},
    {"shift",       KEY_LEFTSHIFT,   0},
    {"leftshift",   KEY_LEFTSHIFT,   0},
    {"left_shift",  KEY_LEFTSHIFT,   0},
    {"rightshift",  KEY_RIGHTSHIFT,  0},
    {"right_shift", KEY_RIGHTSHIFT,  0},
    {"ctrl",        KEY_LEFTCTRL,    0},
    {"leftctrl",    KEY_LEFTCTRL,    0},
    {"left_ctrl",   KEY_LEFTCTRL,    0},
    {"rightctrl",   KEY_RIGHTCTRL,   0},
    {"right_ctrl",  KEY_RIGHTCTRL,   0},
    {"alt",         KEY_LEFTALT,     0},
    {"leftalt",     KEY_LEFTALT,     0},
    {"left_alt",    KEY_LEFTALT,     0},
    {"rightalt",    KEY_RIGHTALT,    0},
    {"right_alt",   KEY_RIGHTALT,    0},
    {"backspace",   KEY_BACKSPACE,   0},
    {"enter",       KEY_ENTER,       0},
    {"pageup",      KEY_PAGEUP,      0},
    {"page_up",     KEY_PAGEUP,      0},
    {"pagedown",    KEY_PAGEDOWN,    0},
    {"page_down",   KEY_PAGEDOWN,    0},
    {"insert",      KEY_INSERT,      0},
    {"delete",      KEY_DELETE,      0},
    {"capslock",    KEY_CAPSLOCK,    0},
    {"tab",         KEY_TAB,         0},
    {"pause",       KEY_PAUSE,       0},
    {"menu",        KEY_MENU,        0},

    // normal keyboard
    {"a", KEY_A,        0},
    {"b", KEY_B,        0},
    {"c", KEY_C,        0},
    {"d", KEY_D,        0},
    {"e", KEY_E,        0},
    {"f", KEY_F,        0},
    {"g", KEY_G,        0},
    {"h", KEY_H,        0},
    {"i", KEY_I,        0},
    {"j", KEY_J,        0},
    {"k", KEY_K,        0},
    {"l", KEY_L,        0},
    {"m", KEY_M,        0},
    {"n", KEY_N,        0},
    {"o", KEY_O,        0},
    {"p", KEY_P,        0},
    {"q", KEY_Q,        0},
    {"r", KEY_R,        0},
    {"s", KEY_S,        0},
    {"t", KEY_T,        0},
    {"u", KEY_U,        0},
    {"v", KEY_V,        0},
    {"w", KEY_W,        0},
    {"x", KEY_X,        0},
    {"y", KEY_Y,        0},
    {"z", KEY_Z,        0},

    // uppercase
    {"A", KEY_A, MOD_SHIFT},
    {"B", KEY_B, MOD_SHIFT},
    {"C", KEY_C, MOD_SHIFT},
    {"D", KEY_D, MOD_SHIFT},
    {"E", KEY_E, MOD_SHIFT},
    {"F", KEY_F, MOD_SHIFT},
    {"G", KEY_G, MOD_SHIFT},
    {"H", KEY_H, MOD_SHIFT},
    {"I", KEY_I, MOD_SHIFT},
    {"J", KEY_J, MOD_SHIFT},
    {"K", KEY_K, MOD_SHIFT},
    {"L", KEY_L, MOD_SHIFT},
    {"M", KEY_M, MOD_SHIFT},
    {"N", KEY_N, MOD_SHIFT},
    {"O", KEY_O, MOD_SHIFT},
    {"P", KEY_P, MOD_SHIFT},
    {"Q", KEY_Q, MOD_SHIFT},
    {"R", KEY_R, MOD_SHIFT},
    {"S", KEY_S, MOD_SHIFT},
    {"T", KEY_T, MOD_SHIFT},
    {"U", KEY_U, MOD_SHIFT},
    {"V", KEY_V, MOD_SHIFT},
    {"W", KEY_W, MOD_SHIFT},
    {"X", KEY_X, MOD_SHIFT},
    {"Y", KEY_Y, MOD_SHIFT},
    {"Z", KEY_Z, MOD_SHIFT},

    {"1", KEY_1,         0},
    {"2", KEY_2,         0},
    {"3", KEY_3,         0},
    {"4", KEY_4,         0},
    {"5", KEY_5,         0},
    {"6", KEY_6,         0},
    {"7", KEY_7,         0},
    {"8", KEY_8,         0},
    {"9", KEY_9,         0},
    {"0", KEY_0,         0},
    {")", KEY_0, MOD_SHIFT},
    {"!", KEY_1, MOD_SHIFT},
    {"@", KEY_2, MOD_SHIFT},
    {"#", KEY_3, MOD_SHIFT},
    {"$", KEY_4, MOD_SHIFT},
    {"^", KEY_6, MOD_SHIFT},
    {"%", KEY_5, MOD_SHIFT},
    {"&", KEY_7, MOD_SHIFT},
    {"*", KEY_8, MOD_SHIFT},
    {"(", KEY_9, MOD_SHIFT},

    {"-", KEY_MINUS,         0},
    {"+", KEY_EQUAL, MOD_SHIFT},

    {"f1",  KEY_F1,  0},
    {"f2",  KEY_F2,  0},
    {"f3",  KEY_F3,  0},
    {"f4",  KEY_F4,  0},
    {"f5",  KEY_F5,  0},
    {"f6",  KEY_F6,  0},
    {"f7",  KEY_F7,  0},
    {"f8",  KEY_F8,  0},
    {"f9",  KEY_F9,  0},
    {"f10", KEY_F10, 0},
    {"f11", KEY_F11, 0},
    {"f12", KEY_F12, 0},

    {"\'", KEY_APOSTROPHE,         0},
    {"\"", KEY_APOSTROPHE, MOD_SHIFT},

    {";", KEY_SEMICOLON,           0},
    {"/", KEY_SLASH,               0},
    {".", KEY_DOT,                 0},
    {",", KEY_COMMA,               0},
    {"`", KEY_GRAVE,               0},
    {"=", KEY_EQUAL,               0},
    {"[", KEY_LEFTBRACE,           0},
    {"]", KEY_RIGHTBRACE,          0},
    {"\\", KEY_BACKSLASH,          0},

    {":", KEY_SEMICOLON,   MOD_SHIFT},
    {"?", KEY_SLASH,       MOD_SHIFT},
    {"<", KEY_COMMA,       MOD_SHIFT},
    {">", KEY_DOT,         MOD_SHIFT},
    {"~", KEY_GRAVE,       MOD_SHIFT},
    {"|", KEY_BACKSLASH,   MOD_SHIFT},
    {"{", KEY_LEFTBRACE,   MOD_SHIFT},
    {"}", KEY_RIGHTBRACE,  MOD_SHIFT},
    {"_", KEY_MINUS,       MOD_SHIFT},
};



const keyboard_values *find_keyboard(const char *key)
{
    int num_pairs = sizeof(keyboard_codes) / sizeof(keyboard_codes[0]);

    for (int i=0; i < num_pairs; i++)
    {
        if (strcmp(key, keyboard_codes[i].str) == 0)
            return &keyboard_codes[i];
    }

    for (int i=0; i < num_pairs; i++)
    {
        if (strcasecmp(key, keyboard_codes[i].str) == 0)
            return &keyboard_codes[i];
    }

    return NULL;
}


const char *find_keycode(short keycode)
{
    int num_pairs = sizeof(keyboard_codes) / sizeof(keyboard_codes[0]);

    for (int i=0; i < num_pairs; i++)
    {
        if (keycode == keyboard_codes[i].keycode)
            return keyboard_codes[i].str;
    }

    return "(null)";
}

const button_match button_codes[] = {
    {"a", GBTN_A},
    {"b", GBTN_B},
    {"x", GBTN_X},
    {"y", GBTN_Y},

    {"l1", GBTN_L1},
    {"l2", GBTN_L2},
    {"l3", GBTN_L3},

    {"r1", GBTN_R1},
    {"r2", GBTN_R2},
    {"r3", GBTN_R3},

    {"start",  GBTN_START},
    {"back",   GBTN_BACK},
    {"select", GBTN_BACK},
    {"guide",  GBTN_GUIDE},

    {"up",    GBTN_DPAD_UP},
    {"down",  GBTN_DPAD_DOWN},
    {"left",  GBTN_DPAD_LEFT},
    {"right", GBTN_DPAD_RIGHT},

    {"left_analog_up",    GBTN_LEFT_ANALOG_UP},
    {"left_analog_down",  GBTN_LEFT_ANALOG_DOWN},
    {"left_analog_left",  GBTN_LEFT_ANALOG_LEFT},
    {"left_analog_right", GBTN_LEFT_ANALOG_RIGHT},

    {"right_analog_up",    GBTN_RIGHT_ANALOG_UP},
    {"right_analog_down",  GBTN_RIGHT_ANALOG_DOWN},
    {"right_analog_left",  GBTN_RIGHT_ANALOG_LEFT},
    {"right_analog_right", GBTN_RIGHT_ANALOG_RIGHT},

    {"dpad",         GBTN_DPAD},
    {"left_analog",  GBTN_LEFT_ANALOG},
    {"right_analog", GBTN_RIGHT_ANALOG},
};

button_match button_hotkey = {"hotkey", GBTN_BACK};

void set_hotkey(int gbtn)
{
    if (gbtn < 0 || gbtn > GBTN_MAX)
        return;

    current_state.hotkey_gbtn = gbtn;
    button_hotkey.gbtn = gbtn;
}

const button_match *find_button(const char *key)
{
    int num_pairs = sizeof(button_codes) / sizeof(button_codes[0]);

    for (int i=0; i < num_pairs; i++)
    {
        if (strcasecmp(key, button_codes[i].str) == 0)
            return &button_codes[i];
    }

    if (strcasecmp(key, button_hotkey.str) == 0)
    {
        return &button_hotkey;
    }

    return NULL;
}
