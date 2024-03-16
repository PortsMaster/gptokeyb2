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

gptokeyb_state current_state;


void state_init()
{
    memset((void*)&current_state, '\0', sizeof(gptokeyb_state));

    set_hotkey(GBTN_BACK);

    current_state.running = true;

    current_state.repeat_delay = SDL_DEFAULT_REPEAT_DELAY;
    current_state.repeat_rate = SDL_DEFAULT_REPEAT_INTERVAL;

    current_state.dpad_mouse_step = 5;

    current_state.deadzone_mode = DZ_DEFAULT;
    current_state.deadzone_scale = 512;

    current_state.deadzone   = 15000;
    current_state.deadzone_y = 15000;
    current_state.deadzone_x = 15000;
    current_state.deadzone_triggers = 3000;
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

    for (int i = 0; i < gptokeyb_config_depth; i++) {
        GPTK2_DEBUG("  ");
    }
    GPTK2_DEBUG("push_state: %s\n", new_config->name);
    config_stack[++gptokeyb_config_depth] = new_config;
}

void set_state(gptokeyb_config *new_config)
{
    for (int i = 0; i < gptokeyb_config_depth; i++) {
        GPTK2_DEBUG("  ");
    }
    GPTK2_DEBUG("set_state: %s\n", new_config->name);
    config_stack[gptokeyb_config_depth] = new_config;
}

void pop_state()
{
    if (gptokeyb_config_depth == 0)
        return;

    for (int i = 0; i < gptokeyb_config_depth; i++) {
        GPTK2_DEBUG("  ");
    }
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

Uint32 held_for(int btn)
{
    if (!is_pressed(btn))
        return 0;

    return (SDL_GetTicks() - current_state.held_since[btn]);
}

void state_update()
{   /* This updates the internal state machine.
     *
     * This handles things like START + SELECT to quit, button repeating.
     */
    Uint32 current_ticks = SDL_GetTicks();

    if (is_pressed(GBTN_START) && is_pressed(current_state.hotkey_gbtn))
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
        if (!current_state.in_repeat[btn])
            continue;

        if (!is_pressed(btn))
            continue;

        if (!SDL_TICKS_PASSED(current_ticks, current_state.next_repeat[btn]))
            continue;

        current_state.next_repeat[btn] = (current_ticks + current_state.repeat_rate);

        // release button
        update_button(btn, false);

        // press button
        current_state.in_repeat[btn] = true;
        current_state.last_pressed[btn] = false;
        update_button(btn, true);
    }

    if (!current_left_analog_as_mouse && !current_right_analog_as_mouse)
    {
        current_state.mouse_x = 0;
        current_state.mouse_x = 0;
    }
}

gptokeyb_config *state_active()
{
    return config_stack[current_depth];
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
    Uint32 current_ticks = SDL_GetTicks();

    gptokeyb_config *config = config_stack[gptokeyb_config_depth];

    const BUTTON_MAP *button = state_button(btn);

    if (button == NULL)
        return;

    current_state.pressed[btn] = pressed;

    if (was_pressed(btn))
    {
        if (!current_state.in_repeat[btn])
        {
            current_state.held_since[btn] = current_ticks;
        }

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
        {   // this way we can always clear the mouse_slow flag if the state changes.
            current_state.mouse_slow |= (1<<btn);
        }
        else if (GBTN_IS_DPAD(btn) && current_dpad_as_mouse)
        {   // this way we can always clear the mouse_move flag if the state changes.
            current_state.mouse_move |= (1<<btn);
        }
        else if (button->repeat && !current_state.in_repeat[btn])
        {
            current_state.in_repeat[btn] = true;
            current_state.next_repeat[btn] = (current_ticks + current_state.repeat_delay);
        }

        if (button->keycode != 0)
        {
            GPTK2_DEBUG("PRESS   '%s' -> '%s'\n", gbtn_names[btn], find_keycode(button->keycode));
            emitKey(button->keycode, true, button->modifier);
        }
    }
    else if (was_released(btn))
    {
        if (current_state.pop_held[btn])
        {
            pop_state();
            current_state.pop_held[btn] = false;
        }

        // Always clear the state of a mouse button if it is released.
        current_state.mouse_slow &= ~(1<<btn);
        current_state.mouse_move &= ~(1<<btn);
        current_state.in_repeat[btn] = false;

        if (button->keycode != 0)
        {
            GPTK2_DEBUG("RELEASE '%s' -> '%s'\n", gbtn_names[btn], find_keycode(button->keycode));
            emitKey(button->keycode, false, button->modifier);
        }
    }
}
