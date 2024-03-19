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

struct _fn_data_store
{
    struct _fn_data_store *next;

    int   fn_id;
    void *data;
};


static int global_function_id = 0;

void functions_init()
{
}

void functions_quit()
{
}


void function_store_set(fn_data_store **store, int fn_id, void *data)
{
    fn_data_store *current = *store;
    while (current != NULL)
    {
        if (current->fn_id == fn_id)
        {
            current->data = data;
            return;
        }
        current = current->next;
    }

    // If the function state with the given fnc_id doesn't exist, create a new one
    fn_data_store *new_state = (fn_data_store *)gptk_malloc(sizeof(fn_data_store));

    new_state->fn_id = fn_id;
    new_state->data = data;
    new_state->next = *store;
    *store = new_state;
}

void *function_store_get(fn_data_store **store, int fn_id)
{
    fn_data_store *current = *store;
    while (current != NULL)
    {
        if (current->fn_id == fn_id)
            return current->data;

        current = current->next;
    }

    return NULL;
}

void function_store_clear(fn_data_store **store, int fn_id)
{
    fn_data_store *current = *store;
    fn_data_store *prev = NULL;

    while (current != NULL)
    {
        if (current->fn_id == fn_id)
        {
            if (prev != NULL)
            {
                prev->next = current->next;
            }
            else
            {
                *store = current->next;
            }

            free(current);
            return;
        }

        prev = current;
        current = current->next;
    }
}

void function_state_clear_all(fn_data_store **store)
{
    fn_data_store *current = *store;
    fn_data_store *next;

    while (current != NULL)
    {
        next = current->next;
        free(current);
        current = next;
    }

    *store = NULL;
}


void  function_global_configure(const char *name, const char *value)
{
    GPTK2_DEBUG("FUNC: global %s -> %s\n", name, value);
}

void  function_config_configure(gptokeyb_config *config, const char *name, const char *value)
{
    GPTK2_DEBUG("FUNC: config[%s] %s -> %s\n", config->name, name, value);
}

void  function_button_configure(gptokeyb_config *config, int btn, const char *name, const char *value, const char *more)
{
    GPTK2_DEBUG("FUNC: config[%s][%s] %s -> %s\n", config->name, gbtn_names[btn], name, value);
}
