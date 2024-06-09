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
bool current_dpad_as_mouse = false;
bool current_left_analog_as_mouse = false;
bool current_right_analog_as_mouse = false;


void state_init()
{
    memset((void*)&current_state, '\0', sizeof(gptokeyb_state));

    set_hotkey(GBTN_BACK);

    current_state.running = true;

    current_state.repeat_delay = SDL_DEFAULT_REPEAT_DELAY;
    current_state.repeat_rate  = SDL_DEFAULT_REPEAT_INTERVAL;

    current_state.dpad_mouse_step  = 5;
    current_state.mouse_slow_scale = 50;

    current_state.deadzone_mode  = DZ_DEFAULT;
    current_state.deadzone_scale = 512;

    current_state.deadzone_x = 1000;
    current_state.deadzone_y = 1000;
    current_state.deadzone_triggers = 3000;

    current_state.dpad_mouse_normalize = true;
}


void push_temp_state(gptokeyb_config *new_config, int btn)
{
    config_temp_stack[btn] = new_config;
    config_temp_stack_order[btn] = ++config_temp_stack_order_id;

    state_change_update();
}


void pop_temp_state(int btn)
{
    bool all_done = true;
    config_temp_stack[btn] = NULL;
    config_temp_stack_order[btn] = 0;

    for (int sbtn=0; sbtn < GBTN_MAX; sbtn++)
    {
        if (config_temp_stack[sbtn] != NULL)
        {
            all_done = false;
            break;
        }
    }

    if (all_done)
        config_temp_stack_order_id = 0;

    state_change_update();
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

#ifdef GPTK2_DEBUG_ENABLED
    for (int i = 0; i < gptokeyb_config_depth; i++) {
        printf("  ");
    }

    printf("push_state: %s\n", new_config->name);
#endif

    config_stack[++gptokeyb_config_depth] = new_config;

    state_change_update();
}

void set_state(gptokeyb_config *new_config)
{
#ifdef GPTK2_DEBUG_ENABLED
    for (int i = 0; i < gptokeyb_config_depth; i++) {
        printf("  ");
    }
    printf("set_state: %s\n", new_config->name);
#endif

    config_stack[gptokeyb_config_depth] = new_config;

    state_change_update();
}

void pop_state()
{
    if (gptokeyb_config_depth == 0)
        return;

#ifdef GPTK2_DEBUG_ENABLED
    for (int i = 0; i < gptokeyb_config_depth; i++) {
        printf("  ");
    }
    printf("pop_state: %s\n", config_stack[gptokeyb_config_depth]->name);
#endif

    gptokeyb_config_depth--;

    state_change_update();
}


bool is_pressed(int btn)
{   // returns tree if button is down
    if (btn < 0 || btn > GBTN_MAX)
        return false;

    return (current_state.pressed & (1<<btn)) != 0;
}

bool was_pressed(int btn)
{   // this will only activate once per button press
    if (btn < 0 || btn > GBTN_MAX)
        return false;

    return (((current_state.pressed & (1<<btn)) != 0) && ((current_state.last_pressed & (1<<btn)) == 0));
}

bool was_released(int btn)
{   // will only activate if the button was released
    if (btn < 0 || btn > GBTN_MAX)
        return false;

    return (((current_state.pressed & (1<<btn)) == 0) && ((current_state.last_pressed & (1<<btn)) != 0));
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

    current_state.last_pressed = current_state.pressed;

    for (int btn=0; btn < GBTN_MAX; btn++)
    {
        if ((current_state.in_repeat & (1<<btn)) == 0)
            continue;

        if (!is_pressed(btn))
            continue;

        if (!SDL_TICKS_PASSED(current_ticks, current_state.next_repeat[btn]))
            continue;

        // release button
        update_button(btn, false);

        // press button
        current_state.in_repeat    |=  (1<<btn);
        current_state.last_pressed &= ~(1<<btn);
        update_button(btn, true);

        current_state.next_repeat[btn] = (current_ticks + current_state.repeat_rate);
    }

    if (!current_left_analog_as_mouse && !current_right_analog_as_mouse)
    {
        current_state.mouse_x = 0;
        current_state.mouse_x = 0;
    }
}


void state_change_update()
{   // check as mouse_move and input set stuff.

    #define NOT_FOUND_DPADS (!found_dpad_as_mouse || !found_left_analog_as_mouse || !found_right_analog_as_mouse)

    #define NOT_FOUND_INPUT_SETS ((found_charset == NULL) && (found_wordset == NULL))

    bool found_dpad_as_mouse = false;
    bool found_left_analog_as_mouse = false;
    bool found_right_analog_as_mouse = false;

    const char *found_charset = NULL;
    const char *found_wordset = NULL;

    // check temp stacks
    int order_id = config_temp_stack_order_id;
    while (order_id > 0)
    {
        for (int sbtn=0; sbtn < GBTN_MAX; sbtn++)
        {
            if (config_temp_stack[sbtn] == NULL)
                continue;

            if (config_temp_stack_order[sbtn] != order_id)
                continue;

            gptokeyb_config *current = config_temp_stack[sbtn];

            if (NOT_FOUND_INPUT_SETS)
            {
                found_charset = current->charset;
                found_wordset = current->wordset;
            }

            if (NOT_FOUND_DPADS)
            {
                if (!found_dpad_as_mouse && current->dpad_as_mouse != MOUSE_MOVEMENT_PARENT)
                {
                    current_dpad_as_mouse = (current->dpad_as_mouse == MOUSE_MOVEMENT_ON);
                    found_dpad_as_mouse = true;
                }

                if (!found_left_analog_as_mouse && current->left_analog_as_mouse != MOUSE_MOVEMENT_PARENT)
                {
                    current_left_analog_as_mouse = (current->left_analog_as_mouse == MOUSE_MOVEMENT_ON);
                    found_left_analog_as_mouse = true;
                }

                if (!found_right_analog_as_mouse && current->right_analog_as_mouse != MOUSE_MOVEMENT_PARENT)
                {
                    current_right_analog_as_mouse = (current->right_analog_as_mouse == MOUSE_MOVEMENT_ON);
                    found_right_analog_as_mouse = true;
                }
            }
        }

        order_id--;
    }

    int current_depth = gptokeyb_config_depth;

    while (current_depth >= 0)
    {
        if (NOT_FOUND_INPUT_SETS)
        {
            found_charset = config_stack[current_depth]->charset;
            found_wordset = config_stack[current_depth]->wordset;
        }

        if (NOT_FOUND_DPADS)
        {
            gptokeyb_config *current = config_stack[current_depth];

            if (!found_dpad_as_mouse && current->dpad_as_mouse != MOUSE_MOVEMENT_PARENT)
            {
                current_dpad_as_mouse = (current->dpad_as_mouse == MOUSE_MOVEMENT_ON);
                found_dpad_as_mouse = true;
            }

            if (!found_left_analog_as_mouse && current->left_analog_as_mouse != MOUSE_MOVEMENT_PARENT)
            {
                current_left_analog_as_mouse = (current->left_analog_as_mouse == MOUSE_MOVEMENT_ON);
                found_left_analog_as_mouse = true;
            }

            if (!found_right_analog_as_mouse && current->right_analog_as_mouse != MOUSE_MOVEMENT_PARENT)
            {
                current_right_analog_as_mouse = (current->right_analog_as_mouse == MOUSE_MOVEMENT_ON);
                found_right_analog_as_mouse = true;
            }
        }

        current_depth--;
    }

    if (found_charset)
    {
        input_load_char_set(found_charset);
    }
    else if (found_wordset)
    {
        input_load_word_set(found_wordset);
    }
    else
    {
        input_stop();
    }

    if (!found_dpad_as_mouse)
        current_dpad_as_mouse = false;

    if (!found_left_analog_as_mouse)
        current_left_analog_as_mouse = false;

    if (!found_right_analog_as_mouse)
        current_right_analog_as_mouse = false;
}


const gptokeyb_button *state_button(int btn)
{   // resolve a button through parent states.
    gptokeyb_button *button;

    // check temp states
    int order_id = config_temp_stack_order_id;
    while (order_id > 0)
    {
        for (int sbtn=0; sbtn < GBTN_MAX; sbtn++)
        {
            if (config_temp_stack[sbtn] == NULL)
                continue;

            if (config_temp_stack_order[sbtn] != order_id)
                continue;

            gptokeyb_config *current = config_temp_stack[sbtn];

            if (current->button[btn].action == ACT_PARENT)
                continue;

            GPTK2_DEBUG("found temp[%s] -> %s\n", gbtn_names[sbtn], gbtn_names[btn]);

            return &current->button[btn];
        }

        order_id--;
    }

    // check stack
    int current_depth = gptokeyb_config_depth;

    while (current_depth >= 0)
    {
        gptokeyb_config *config = config_stack[current_depth];

        button = &config->button[btn];

        if (button->action != ACT_PARENT)
        {
            GPTK2_DEBUG("found stack[%d] -> %s\n", current_depth, gbtn_names[btn]);
            return button;
        }

        current_depth--;
    }

    return NULL;
}


void update_button(int btn, bool pressed)
{
    Uint32 btn_mask = (1<<btn);
    Uint32 current_ticks = SDL_GetTicks();
    const gptokeyb_button *button;

    if (pressed)
        current_state.pressed |=  btn_mask;
    else
        current_state.pressed &= ~btn_mask;

    if (was_pressed(btn))
    {
        // GPTK2_DEBUG("%s -> %s\n", gbtn_names[btn], (pressed ? "pressed" : "released"));

        if ((current_state.in_repeat & btn_mask) != 0)
        {   // if we're in repeat we get the held button.
            current_state.held_since[btn] = current_ticks;
            button = current_state.button_held[btn];
        }
        else
        {   // Otherwise we find it out from the stack.
            button = state_button(btn);
            current_state.button_held[btn] = button;
        }

        if (button == NULL)
            return;

        if (button->action == ACT_STATE_POP)
        {
            pop_state();
        }

        else if (button->action >= ACT_STATE_HOLD)
        {   // change control state
            if (!(current_state.in_repeat & btn_mask))
            {
                if (button->action == ACT_STATE_HOLD)
                {
                    push_temp_state(button->cfg_map, btn);
                    current_state.pop_held |= btn_mask;
                }
                else if (button->action == ACT_STATE_SET)
                {
                    set_state(button->cfg_map);
                }
                else
                {
                    push_state(button->cfg_map);
                }
            }

            if (button->keycode != 0)
            {
                GPTK2_DEBUG("PRESSED '%s' -> '%s'\n", gbtn_names[btn], find_keycode(button->keycode));
                emitKey(button->keycode, true, button->modifier);

                if (button->repeat && !(current_state.in_repeat & btn_mask))
                {
                    current_state.in_repeat |= btn_mask;
                    current_state.next_repeat[btn] = (current_ticks + current_state.repeat_delay);
                }
            }
        }
        else if (button->action == ACT_SPECIAL && button->special == SPC_MOUSE_SLOW)
        {   // this way we can always clear the mouse_slow flag if the state changes.
            current_state.mouse_slow |= btn_mask;
        }
        else if (button->action == ACT_SPECIAL && button->special >= SPC_ADD_LETTER)
        {   // special controls
            printf("%d -> %d -> %d: %d\n", btn, button->action, button->special,(int)input_active());
            switch (button->special)
            {
            case SPC_ADD_LETTER:
                if (input_active())
                    input_add_letter();
                break;

            case SPC_REM_LETTER:
                if (input_active())
                    input_rem_letter();
                break;

            case SPC_NEXT_LETTER:
                if (input_active())
                    input_next_letter(1);
                break;

            case SPC_PREV_LETTER:
                if (input_active())
                    input_prev_letter(1);
                break;

            case SPC_NEXT_WORD:
                if (input_active())
                    input_next_word(1);
                break;

            case SPC_PREV_WORD:
                if (input_active())
                    input_prev_word(1);
                break;

            case SPC_UPPER_CASE:
                if (input_active())
                    input_upper_case();
                break;

            case SPC_LOWER_CASE:
                if (input_active())
                    input_lower_case();
                break;

            case SPC_TOGGLE_CASE:
                if (input_active())
                    input_toggle_case();
                break;

            case SPC_ACCEPT_INPUT:
                if (input_active())
                    input_accept();
                break;

            case SPC_CANCEL_INPUT:
                if (input_active())
                    input_cancel();
                break;

            default:
                break;
            }
        }
        else if (GBTN_IS_DPAD(btn) && current_dpad_as_mouse)
        {   // this way we can always clear the mouse_move flag if the state changes.
            current_state.mouse_move |= btn_mask;
        }
        else if (button->repeat && !(current_state.in_repeat & btn_mask))
        {
            current_state.in_repeat |= btn_mask;
            current_state.next_repeat[btn] = (current_ticks + current_state.repeat_delay);
        }

        if (button->keycode != 0)
        {
            GPTK2_DEBUG("PRESSED '%s' -> '%s'\n", gbtn_names[btn], find_keycode(button->keycode));
            emitKey(button->keycode, true, button->modifier);
        }
    }
    else if (was_released(btn))
    {
        button = current_state.button_held[btn];

        // Not repeating this button, lets clear the held button
        if ((current_state.in_repeat & btn_mask) == 0)
            current_state.button_held[btn] = NULL;

        if (button == NULL)
            return;

        // GPTK2_DEBUG("%s -> %s\n", gbtn_names[btn], (pressed ? "pressed" : "released"));

        if ((current_state.pop_held & btn_mask) != 0)
        {
            pop_temp_state(btn);
            current_state.pop_held &= ~btn_mask;
        }

        // Always clear the state of a mouse button if it is released.
        current_state.mouse_slow &= ~btn_mask;
        current_state.mouse_move &= ~btn_mask;
        current_state.in_repeat  &= ~btn_mask;

        if (button->keycode != 0)
        {
            GPTK2_DEBUG("RELEASE '%s' -> '%s'\n", gbtn_names[btn], find_keycode(button->keycode));
            emitKey(button->keycode, false, button->modifier);
        }
    }
}
