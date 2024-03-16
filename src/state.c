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

extern gptokeyb_state current_state;

gptokeyb_config *config_stack[CFG_STACK_MAX];
int gptokeyb_config_depth = 0;

void state_init()
{
    memset((void*)&current_state, '\0', sizeof(gptokeyb_state));

    current_state.hotkey = GBTN_BACK;
    current_state.running = true;
}

void push_state(gptokeyb_config *new_config)
{
    if (gptokeyb_config_depth >= (CFG_STACK_MAX - 1))
    {
        fprintf(stderr, "maximum state depth reached.\n");
        for (int i=0; i < CFG_STACK_MAX; i++)
        {
            fprintf(stderr, "%02d) '%s'\n", i, config_stack[i]->name);
        }
        return;
    }

    GPTK2_DEBUG("push_state: %s\n", new_config->name);
    config_stack[gptokeyb_config_depth++] = new_config;
}

void set_state(gptokeyb_config *new_config)
{
    GPTK2_DEBUG("set_state: %s\n", new_config->name);
    config_stack[gptokeyb_config_depth] = new_config;
}

void pop_state()
{
    if (gptokeyb_config_depth == 0)
        return;

    GPTK2_DEBUG("pop_state: %s\n", config_stack[gptokeyb_config_depth]->name);
    gptokeyb_config_depth--;
}


bool is_pressed(int btn)
{   // returns tree if button is down
    if (btn < 0 || btn > GBTN_MAX)
        return false;

    return current_state.pressed[btn];
}

bool was_pressed(int btn)
{   // this will only activate once per button press
    if (btn < 0 || btn > GBTN_MAX)
        return false;

    return (current_state.pressed[btn] && !current_state.last_pressed[btn]);
}

bool was_released(int btn)
{   // will only activate if the button was released
    if (btn < 0 || btn > GBTN_MAX)
        return false;

    return (!current_state.pressed[btn] && current_state.last_pressed[btn]);
}

Uint64 held_for(int btn)
{
    Uint64 current_ticks = SDL_GetTicks64();

    if (!is_pressed(btn))
        return 0;

    return (current_ticks - current_state.held_since[btn])
}

void state_update()
{   /* This updates the internal state machine.
     *
     * This handles things like START + SELECT to quit, button repeating.
     */
    Uint64 current_ticks = SDL_GetTicks64();

    if (is_pressed(GBTN_START) && is_pressed(current_state.hotkey))
    {
        if (process_kill())
            current_state.running = false;
    }

    for (int btn=0; btn < GBTN_MAX; btn++)
    {
        current_state.last_pressed[btn] = current_state.pressed[btn];
    }

    for (int btn=0; btn < GBTN_MAX; btn++)
    {
        if (!current_state.want_repeat[btn])
            continue;

        if (!is_pressed(btn))
            continue;

        if (current_state.next_repeat[btn] < current_ticks)
            continue;

        current_state.next_repeat[btn] = (current_ticks + current_state.repeat_rate);

        // release button
        update_button(btn, false);

        // press button
        current_state.last_pressed[btn] = false;
        update_button(btn, true);
    }
}


const BUTTON_MAP *state_button(int btn)
{   // resolve a button through parent states.
    BUTTON_MAP *button;
    int current_depth = gptokeyb_config_depth;

    while (current_depth >= 0)
    {
        gptokeyb_config *config = config_stack[current_depth];

        button = &config->button[btn];

        if (button->action != ACT_PARENT)
            return button;

        current_depth--;
    }

    return NULL;
}


void update_button(int btn, bool pressed)
{
    gptokeyb_config *config = config_stack[gptokeyb_config_depth];

    const BUTTON_MAP *button = state_button(btn);

    if (button == NULL)
        return;

    current_state.pressed[btn] = pressed;
    current_state
    if (was_pressed(btn))
    {
        if (button->action == ACT_STATE_POP)
        {
            pop_state();
        }
        else if (button->action >= ACT_STATE_HOLD)
        {   // change control state
            if (button->action == ACT_STATE_HOLD)
                current_state.pop_held[btn] = true;

            if (button->action == ACT_STATE_SET)
                set_state(button->cfg_map);
            else
                push_state(button->cfg_map);
        }
        else if (button->action == ACT_MOUSE_SLOW)
        {
            current_state.mouse_slow = true;
        }
        else if (button->repeat)
        {

        }
    }

    else if (was_released(btn))
    {
        if (current_state.pop_held[btn])
        {
            pop_state();
            current_state.pop_held[btn] = false;
        }
        else if (button->action == ACT_MOUSE_SLOW)
        {
            current_state.mouse_slow = false;
        }
    }
}
