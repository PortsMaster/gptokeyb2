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

// THANKS CHATGPT
bool strendswith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;

    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);

    if (lensuffix > lenstr)
        return 0;

    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

bool strcaseendswith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;

    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);

    if (lensuffix > lenstr)
        return 0;

    return strncasecmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

bool strstartswith(const char *str, const char *prefix)
{
    return strncmp(prefix, str, strlen(prefix)) == 0;
}

bool strcasestartswith(const char *str, const char *prefix)
{
    return strncasecmp(prefix, str, strlen(prefix)) == 0;
}

int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && *s2)
    {
        char c1 = tolower(*s1);
        char c2 = tolower(*s2);

        if (c1 != c2)
        {
            return c1 - c2;
        }

        s1++;
        s2++;
    }

    return *s1 - *s2;
}

int strncasecmp(const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        char c1 = tolower(s1[i]);
        char c2 = tolower(s2[i]);

        if (c1 != c2 || c1 == '\0' || c2 == '\0')
        {
            return c1 - c2;
        }
    }

    return 0;
}


void emit(int type, int code, int val)
{
    struct input_event ev;

    ev.type = type;
    ev.code = code;
    ev.value = val;
    /* timestamp values below are ignored */
    ev.time.tv_sec = 0;
    ev.time.tv_usec = 0;

    write(uinp_fd, &ev, sizeof(ev));
}

void emitModifier(bool is_pressed, int modifier)
{
    if ((modifier & MOD_SHIFT) != 0)
    {
        emit(EV_KEY, KEY_LEFTSHIFT, is_pressed ? 1 : 0);
        emit(EV_SYN, SYN_REPORT, 0);        
    }

    if ((modifier & MOD_ALT) != 0)
    {
        emit(EV_KEY, KEY_LEFTALT, is_pressed ? 1 : 0);
        emit(EV_SYN, SYN_REPORT, 0);        
    }

    if ((modifier & MOD_CTRL) != 0)
    {
        emit(EV_KEY, KEY_LEFTCTRL, is_pressed ? 1 : 0);
        emit(EV_SYN, SYN_REPORT, 0);        
    }
}

void emitKey(int code, bool is_pressed, int modifier)
{
    if (code == 0)
        return;

    if ((modifier != 0) && is_pressed)
        emitModifier(is_pressed, modifier);

    emit(EV_KEY, code, is_pressed ? 1 : 0);
    emit(EV_SYN, SYN_REPORT, 0);

    if ((modifier != 0) && !(is_pressed))
        emitModifier(is_pressed, modifier);
}

void emitTextInputKey(int code, bool uppercase)
{
    if (uppercase)
    {   //capitalise capital letters by holding shift
        emitKey(KEY_LEFTSHIFT, true, 0);
    }

    emitKey(code, true, 0);
    SDL_Delay(16);
    emitKey(code, false, 0);
    SDL_Delay(16);

    if (uppercase)
    {   //release shift if held
        emitKey(KEY_LEFTSHIFT, false, 0);
    }
}


void emitAxisMotion(int code, int value)
{
    emit(EV_ABS, code, value);
    emit(EV_SYN, SYN_REPORT, 0);
}

void emitMouseMotion(int x, int y)
{
    if (x != 0)
    {
        emit(EV_REL, REL_X, x);
    }
    if (y != 0)
    {
        emit(EV_REL, REL_Y, y);
    }

    if (x != 0 || y != 0)
    {
        emit(EV_SYN, SYN_REPORT, 0);
    }
}

void handleAnalogTrigger(bool is_triggered, bool *was_triggered, int key, int modifier)
{
    if (is_triggered && !(*was_triggered))
    {
        emitKey(key, true, modifier);
    }
    else if (!is_triggered && (*was_triggered))
    {
        emitKey(key, false, modifier);
    }

    *was_triggered = is_triggered;
}




