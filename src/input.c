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


typedef struct {
    short keycode;
    bool  shift;
} character_map;


#define MAX_TEXT_LENGTH 32

const char_set char_sets[] = {
    {"basic",    "ABCDEFGHIJKLMNOPQRSTUVWXYZ .,-_() 0123456789 "},
    {"extended", "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz .,-_() 0123456789 "},
    {"full",     "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz .,-_()@#%&*-+!\"\':;/?~`|{}$^=""[]\\<> 0123456789"},
};

character_map characters[256];


const char_set *find_char_set(const char *name)
{
    for (int i=0; i < (sizeof(char_sets) / sizeof(char_sets[0])); i++)
    {
        if (strcasecmp(name, char_sets[i].name) == 0)
        {
            return &char_sets[i];
        }
    }

    return NULL;
}


void input_init()
{
    const char_set *full_set = find_char_set("full");

    int character_len = strlen(full_set->characters);
    const keyboard_values *keyinfo;
    memset((void*)characters, '\0', sizeof(characters));
    char find_buff[3] = "\0\0\0";

    for (int i = 0; i < character_len; i++)
    {
        find_buff[0] = full_set->characters[i];
        keyinfo = find_keyboard(find_buff);

        if (keyinfo == NULL)
        {
            printf("bad character '%s'\n", keyinfo->str);
            continue;
        }

        characters[full_set->characters[i]].keycode = keyinfo->keycode;
        characters[full_set->characters[i]].shift   = (keyinfo->modifier & MOD_SHIFT) != 0;
    }
}

