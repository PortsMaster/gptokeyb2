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
#include "ini.h"

#define MAX_TEMP_SIZE 1024


enum
{
    CFG_GPTK,
    CFG_CONFIG,
    CFG_CONTROL,
    CFG_OTHER,
    CFG_IGNORE,
};


typedef struct
{
    int state;
    bool config_only;
    char last_section[MAX_CONTROL_NAME];
    gptokeyb_config *current_config;
} config_parser;


gptokeyb_config *root_config = NULL;
gptokeyb_config *config_stack[CFG_STACK_MAX];
gptokeyb_config *config_temp_stack[GBTN_MAX];

char default_control_name[MAX_CONTROL_NAME] = "";
int config_temp_stack_order[GBTN_MAX];
int config_temp_stack_order_id = 0;

int gptokeyb_config_depth = 0;

#define GPTK_HK_FIX_MAX 50
#define GPTK_HK_FIX_MAX_LINE 1024
char *gptk_hk_fix_text[GPTK_HK_FIX_MAX];
int gptk_hk_fix_offset = 0;
int gptk_hk_can_fix = true;

const char *gbtn_names[] = {
    "a",
    "b",
    "x",
    "y",

    "l1",
    "l2",
    "l3",

    "r1",
    "r2",
    "r3",

    "start",
    "back",
    "guide",

    "up",
    "down",
    "left",
    "right",

    "left_analog_up",
    "left_analog_down",
    "left_analog_left",
    "left_analog_right",

    "right_analog_up",
    "right_analog_down",
    "right_analog_left",
    "right_analog_right",

    // SPECIAL
    "(max)",

    // SPECIAL
    "dpad",
    "left_analog",
    "right_analog",
};


const char *act_names[] = {
    "(none)",
    "parent",
    "(special)",
    "pop_state",
    "hold_state",
    "push_state",
    "set_state",
};


const char *spc_names[] = {
    "(none)",
    "mouse_slow",
    "add_letter",
    "remove_letter",
    "next_letter",
    "prev_letter",
    "next_word",
    "prev_word",
    "lower_case",
    "tolower",
    "toggle_case",
    "finish_text",
    "cancel_text",
};

const char *ovl_names[] = {
    "(none)",
    "parent",
    "clear",
};

const char *exl_names[] = {
    "false",
    "true",
    "parent",
};

int atoi_between(const char *value, int minimum, int maximum, int default_value)
{
    char *endptr;
    long int result = strtol(value, &endptr, 10); // Assuming base 10

    // Check if strtol encountered an error
    if (endptr == value || *endptr != '\0')
    {
        // Conversion failed or non-numeric characters encountered
        return default_value;
    }

    // Check if the result is within the specified range
    if (result < minimum)
        result = minimum;

    else if (result > maximum)
        result = maximum;

    return (int)result;
}


bool atob_default(const char *value, bool default_value)
{
    if (strcasecmp(value, "true") == 0)
        return true;

    if (strcasecmp(value, "1") == 0)
        return true;

    if (strcasecmp(value, "false") == 0)
        return false;

    if (strcasecmp(value, "0") == 0)
        return false;

    return default_value;
}


int special_button_min(int btn)
{
    if (btn == GBTN_DPAD)
        return GBTN_DPAD_UP;

    if (btn == GBTN_LEFT_ANALOG)
        return GBTN_LEFT_ANALOG_UP;

    if (btn == GBTN_RIGHT_ANALOG)
        return GBTN_RIGHT_ANALOG_UP;

    return 0;
}


int special_button_max(int btn)
{
    if (btn == GBTN_DPAD)
        return GBTN_DPAD_RIGHT+1;

    if (btn == GBTN_LEFT_ANALOG)
        return GBTN_LEFT_ANALOG_RIGHT+1;

    if (btn == GBTN_RIGHT_ANALOG)
        return GBTN_RIGHT_ANALOG_RIGHT+1;

    return 0;
}


void config_init()
{   // Setup config structures.
    root_config = (gptokeyb_config*)gptk_malloc(sizeof(gptokeyb_config));

    root_config->name = string_register("controls");

    gptokeyb_config_depth = 0;

    config_stack[0] = root_config;

    for (int i=1; i < CFG_STACK_MAX; i++)
    {
        config_stack[i] = NULL;
    }

    for (int btn=0; btn < GBTN_MAX; btn++)
    {
        config_temp_stack[btn] = NULL;
        config_temp_stack_order[btn] = 0;
    }

    for (int i=0; i < GPTK_HK_FIX_MAX; i++)
        gptk_hk_fix_text[i] = NULL;
}


void config_quit()
{   // Destroy config structures.
    gptokeyb_config *current = root_config;
    gptokeyb_config *next;

    while (current != NULL)
    {
        next = current->next;

        free(current);
        current = next;
    }

    for (int i=0; i < CFG_STACK_MAX; i++)
    {
        config_stack[i] = NULL;
    }

    for (int i=0; i < gptk_hk_fix_offset; i++)
    {
        free(gptk_hk_fix_text[i]);
        gptk_hk_fix_text[i] = NULL;
    }

    gptk_hk_fix_offset = 0;
}


void config_dump()
{   // Dump all the current configs.
    gptokeyb_config *current = root_config;
    bool need_newline = false;

    // printf("###########################################\n");
    // printf("# CONFIG DUMP\n");
    // printf("\n");

    printf("[config]\n");
    printf("repeat_delay = %" PRIu64 "\n", current_state.repeat_delay);
    printf("repeat_rate = %" PRIu64 "\n", current_state.repeat_rate);
    // printf("mouse_scale = %d\n", current_state.mouse_scale);
    printf("mouse_delay = %d\n", current_state.mouse_slow_scale);
    printf("mouse_slow_scale = %d\n", current_state.mouse_slow_scale);
    printf("deadzone_mode = %s\n", deadzone_mode_str(current_state.deadzone_mode));
    printf("deadzone_scale = %d\n", current_state.deadzone_scale);
    printf("deadzone_x = %d\n", current_state.deadzone_x);
    printf("deadzone_y = %d\n", current_state.deadzone_y);
    printf("deadzone_triggers = %d\n", current_state.deadzone_triggers);
    printf("dpad_mouse_normalize = %s\n", (current_state.dpad_mouse_normalize ? "true" : "false" ));

    dump_word_sets();
    dump_char_sets();

    if (strlen(default_control_name) > 0)
        printf("controls = \"%s\"\n", default_control_name);

    printf("\n");

    while (current != NULL)
    {
        printf("[%s]\n\n", current->name);

        if (current->overlay_mode != OVL_NONE)
        {
            printf("overlay = \"%s\"\n", ovl_names[current->overlay_mode]);
            need_newline = true;
        }

        if (current->charset != NULL)
        {
            printf("charset = \"%s\"\n", current->charset);
            need_newline = true;
        }

        if (current->wordset != NULL)
        {
            printf("wordset = \"%s\"\n", current->wordset);
            need_newline = true;
        }

        if (current->exclusive_mode != EXL_FALSE)
        {
            printf("exclusive = %s\n", exl_names[current->exclusive_mode]);
        }

        for (int btn=0; btn < GBTN_MAX; btn++)
        {
            if ((current->dpad_as_mouse != MOUSE_MOVEMENT_OFF && btn == GBTN_DPAD_UP) ||
                (current->left_analog_as_mouse != MOUSE_MOVEMENT_OFF && btn == GBTN_LEFT_ANALOG_UP) ||
                (current->right_analog_as_mouse != MOUSE_MOVEMENT_OFF && btn == GBTN_RIGHT_ANALOG_UP))
            {
                char *gbnt_name;
                char *gbnt_mode;

                if (btn == GBTN_DPAD_UP)
                {
                    gbnt_name = "dpad";
                    gbnt_mode = ((current->dpad_as_mouse == MOUSE_MOVEMENT_ON) ? "mouse_movement" : "parent");
                }
                else if (btn == GBTN_LEFT_ANALOG_UP)
                {
                    gbnt_name = "left_analog";
                    gbnt_mode = ((current->left_analog_as_mouse == MOUSE_MOVEMENT_ON) ? "mouse_movement" : "parent");
                }
                else if (btn == GBTN_RIGHT_ANALOG_UP)
                {
                    gbnt_name = "right_analog";
                    gbnt_mode = ((current->right_analog_as_mouse == MOUSE_MOVEMENT_ON) ? "mouse_movement" : "parent");
                }

                if (strcmp(gbnt_mode, "parent") == 0 && current->overlay_mode == OVL_PARENT)
                {
                    btn += 3;
                    continue;
                }

                if (need_newline)
                {
                    printf("\n");
                    need_newline = false;
                }

                printf("%s = %s\n", gbnt_name, gbnt_mode);
                need_newline = true;
                btn += 3;
                continue;
            }

            if (current->overlay_mode == OVL_CLEAR && current->button[btn].keycode == 0 && current->button[btn].action == 0)
            {
                if ((btn == GBTN_Y) || (btn == GBTN_R3) || (btn == GBTN_GUIDE) || (btn == GBTN_DPAD_RIGHT) || (btn == GBTN_LEFT_ANALOG_RIGHT))
                    need_newline = true;

                continue;
            }

            if (current->overlay_mode == OVL_PARENT && current->button[btn].action == ACT_PARENT)
            {
                if ((btn == GBTN_Y) || (btn == GBTN_R3) || (btn == GBTN_GUIDE) || (btn == GBTN_DPAD_RIGHT) || (btn == GBTN_LEFT_ANALOG_RIGHT))
                    need_newline = true;

                continue;
            }

            if (need_newline)
            {
                printf("\n");
                need_newline = false;
            }

            printf("%s =", gbtn_names[btn]);

            if (current->button[btn].keycode != 0)
            {
                const char *key_str = find_keycode(current->button[btn].keycode);
                if (strcmp(key_str, "\"") == 0)
                    printf(" \'%s\'", key_str);
                else
                    printf(" \"%s\"", key_str);

                if ((current->button[btn].modifier & MOD_ALT) != 0)
                    printf(" add_alt");

                if ((current->button[btn].modifier & MOD_SHIFT) != 0)
                    printf(" add_shift");

                if ((current->button[btn].modifier & MOD_CTRL) != 0)
                    printf(" add_ctrl");
            }

            if (current->button[btn].action != 0)
            {
                if (current->button[btn].action == ACT_SPECIAL)
                    printf(" %s", spc_names[current->button[btn].special]);

                else if (current->button[btn].cfg_name != NULL)
                    printf(" %s %s", act_names[current->button[btn].action], current->button[btn].cfg_name);

                else
                    printf(" %s", act_names[current->button[btn].action]);
            }

            if (current->button[btn].repeat)
                printf(" repeat");

            printf("\n");

            if ((btn == GBTN_Y) || (btn == GBTN_R3) || (btn == GBTN_GUIDE) || (btn == GBTN_DPAD_RIGHT) || (btn == GBTN_LEFT_ANALOG_RIGHT))
                need_newline = true;
        }

        current = current->next;
        printf("\n");
    }

    // printf("###########################################\n");
}


void config_overlay_clear(gptokeyb_config *current)
{
    current->overlay_mode = OVL_CLEAR;
    current->dpad_as_mouse = MOUSE_MOVEMENT_OFF;
    current->left_analog_as_mouse = MOUSE_MOVEMENT_OFF;
    current->right_analog_as_mouse = MOUSE_MOVEMENT_OFF;
    current->exclusive_mode = EXL_FALSE;

    for (int btn=0; btn < GBTN_MAX; btn++)
    {
        current->button[btn].keycode  = 0;
        current->button[btn].modifier = 0;
        current->button[btn].action   = ACT_NONE;
        current->button[btn].special  = SPC_NONE;
        current->button[btn].repeat   = false;
    }
}

void config_overlay_parent(gptokeyb_config *current)
{
    current->overlay_mode = OVL_PARENT;
    current->dpad_as_mouse = MOUSE_MOVEMENT_PARENT;
    current->left_analog_as_mouse = MOUSE_MOVEMENT_PARENT;
    current->right_analog_as_mouse = MOUSE_MOVEMENT_PARENT;
    current->exclusive_mode = EXL_PARENT;

    for (int btn=0; btn < GBTN_MAX; btn++)
    {
        current->button[btn].keycode  = 0;
        current->button[btn].modifier = 0;
        current->button[btn].action   = ACT_PARENT;
        current->button[btn].special  = SPC_NONE;
        current->button[btn].repeat   = false;
    }
}


void config_overlay_named(gptokeyb_config *current, const char *name)
{
    gptokeyb_config *other = config_find(name);
    if (other == NULL)
    {
        fprintf(stderr, "overlay %s: unable to find config\n", name);
        return;
    }

    if (current == other)
    {
        fprintf(stderr, "overlay %s: unable to overlay to the same config\n", name);
        return;
    }

    current->overlay_mode = OVL_NONE;

    // fprintf(stderr, "overlay %s: \n", other->name);
    current->dpad_as_mouse         = other->dpad_as_mouse;
    current->left_analog_as_mouse  = other->left_analog_as_mouse;
    current->right_analog_as_mouse = other->right_analog_as_mouse;

    current->exclusive_mode        = other->exclusive_mode;

    for (int btn=0; btn < GBTN_MAX; btn++)
    {
        current->button[btn].keycode  = other->button[btn].keycode;
        current->button[btn].modifier = other->button[btn].modifier;
        current->button[btn].action   = other->button[btn].action;
        current->button[btn].special  = other->button[btn].special;
        current->button[btn].repeat   = other->button[btn].repeat;

        if (current->button[btn].action >= ACT_STATE_HOLD)
        {
            current->button[btn].cfg_name = other->button[btn].cfg_name;
            current->map_check = true;
        }
    }
}


gptokeyb_config *config_find(const char *name)
{   // Find a gptokeyb_config by name.
    char nice_name[MAX_CONTROL_NAME];

    // shortcut
    if (strcasecmp("controls", name) == 0)
    {
        // GPTK2_DEBUG("config_find: found %s\n", name);
        return root_config;
    }

    if (!strcasestartswith(name, "controls:"))
    {
        snprintf(nice_name, MAX_CONTROL_NAME-1, "controls:%s", name);
        name=nice_name;
    }

    gptokeyb_config *current = root_config;

    while (current != NULL)
    {
        if (strcasecmp(current->name, name) == 0)
        {
            // GPTK2_DEBUG("config_find: found %s\n", name);
            return current;
        }

        current = current->next;
    }

    // GPTK2_DEBUG("config_find: unable to find %s\n", name);

    return NULL;
}


gptokeyb_config *config_create(const char *name)
{   // find a config, if it doesnt exist, create it.
    gptokeyb_config *result=NULL;

    result = config_find(name);
    if (result != NULL)
        return result;

    result = (gptokeyb_config*)gptk_malloc(sizeof(gptokeyb_config));

    if (!strcasestartswith(name, "controls:"))
    {
        char nice_name[MAX_CONTROL_NAME];
        snprintf(nice_name, MAX_CONTROL_NAME-1, "controls:%s", name);
        result->name = string_register(nice_name);
    }
    else
    {
        result->name = string_register(name);
    }

    // add it to the linked list.
    gptokeyb_config *last_config = root_config;
    while (last_config->next != NULL)
        last_config = last_config->next;

    last_config->next = result;

    // GPTK2_DEBUG("config_create: %s\n", result->name);
    return result;
}

void set_cfg_config(const char *name, const char *value, token_ctx *token_state)
{
    // printf("%s -> %s\n", name, value);

    if (strcasecmp(name, "repeat_delay") == 0)
        current_state.repeat_delay = atoi_between(value, 16, 3000, SDL_DEFAULT_REPEAT_DELAY);

    else if (strcasecmp(name, "repeat_rate") == 0)
        current_state.repeat_rate = atoi_between(value, 16, 3000, SDL_DEFAULT_REPEAT_INTERVAL);

    else if (strcasecmp(name, "mouse_slow_scale") == 0)
        current_state.mouse_slow_scale = atoi_between(value, 1, 100, 50);

    else if (strcasecmp(name, "deadzone_mode") == 0)
        current_state.deadzone_mode = deadzone_get_mode(value);

    else if (strcasecmp(name, "deadzone_scale") == 0)
        current_state.deadzone_scale = atoi_between(value, 1, 32768, 512);

    else if (strcasecmp(name, "mouse_scale") == 0)
        current_state.deadzone_scale = atoi_between(value, 1, 32768, 512);

    else if (strcasecmp(name, "deadzone") == 0)
        current_state.deadzone_x = current_state.deadzone_y = atoi_between(value, 500, 32768, 15000);

    else if (strcasecmp(name, "deadzone_y") == 0)
        current_state.deadzone_y = atoi_between(value, 500, 32768, 1000);

    else if (strcasecmp(name, "deadzone_x") == 0)
        current_state.deadzone_x = atoi_between(value, 500, 32768, 1000);

    else if (strcasecmp(name, "deadzone_triggers") == 0)
        current_state.deadzone_triggers = atoi_between(value, 500, 32768, 3000);

    else if (strcasecmp(name, "dpad_mouse_normalize") == 0)
        current_state.dpad_mouse_normalize = atob_default(value, true);

    else if (strcasecmp(name, "mouse_delay") == 0)
        ((void)0);

    else if (strcasecmp(name, "deadzone_delay") == 0)
        ((void)0);

    else if (strcasecmp(name, "controls") == 0)
        strncpy(default_control_name, value, MAX_CONTROL_NAME - 1);

    else if (strcasecmp(name, "charset") == 0)
    {
        while (value != NULL && strlen(value) == 0)
            value = tokens_next(token_state);

        if (value == NULL)
        {
            fprintf(stderr, "charset used without any name or characters defined.\n");
            tokens_free(token_state);
            return;
        }

        char *chars_name = strdup(value);

        value = tokens_next(token_state);

        while (value != NULL && strlen(value) == 0)
            value = tokens_next(token_state);

        if (value == NULL)
        {
            fprintf(stderr, "charset \"%s\" specified without any characters defined.\n", chars_name);
            free(chars_name);
            return;
        }

        register_char_set(chars_name, value);

        free(chars_name);
        return;
    }
    else if (strcasecmp(name, "wordset") == 0)
    {
        while (value != NULL && strlen(value) == 0)
            value = tokens_next(token_state);

        if (value == NULL)
        {
            fprintf(stderr, "wordset used without any name or words defined.\n");
            return;
        }

        char *words_name = strdup(value);

        value = tokens_next(token_state);

        while (value != NULL && strlen(value) == 0)
            value = tokens_next(token_state);

        if (value == NULL)
        {
            fprintf(stderr, "wordset \"%s\" specified without any words defined.\n", words_name);
            free(words_name);
            return;
        }

        while (value != NULL)
        {
            register_word_set(words_name, value);

            value = tokens_next(token_state);

            while (value != NULL && strlen(value) == 0)
                value = tokens_next(token_state);
        }

        free(words_name);
        return;
    }
#ifdef GPTK2_DEBUG_ENABLED
    else
    {
        GPTK2_DEBUG("# unknown global %s = %s\n", name, value);
    }
#endif
}

static inline void set_btn_as_mouse(int btn, gptokeyb_config *config, int mode)
{
    if (GBTN_IS_DPAD(btn) || btn == GBTN_DPAD)
        config->dpad_as_mouse = mode;

    else if (GBTN_IS_LEFT_ANALOG(btn) || btn == GBTN_LEFT_ANALOG)
        config->left_analog_as_mouse = mode;

    else if (GBTN_IS_RIGHT_ANALOG(btn) || btn == GBTN_RIGHT_ANALOG)
        config->right_analog_as_mouse = mode;
}


void set_btn_config(gptokeyb_config *config, int btn, const char *name, const char *value, token_ctx *token_state)
{   // this parses a keybinding
    /*
     * Keybindings can be in the form:
     *
     *   a = f1
     *   a = add_alt
     */

    bool first_run = true;

    const char *token = value;

    while (token != NULL)
    {
        // printf("%d -> %s -> %s -> %s\n", btn, name, value, token);

        if (strlen(token) == 0)
        {
            token = tokens_next(token_state);
            continue;
        }

        if (strcasecmp(token, "mouse_slow") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            set_btn_as_mouse(btn, config, MOUSE_MOVEMENT_OFF);
            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_MOUSE_SLOW;
        }
        else if (strcasecmp(token, "prev_letter") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_PREV_LETTER;
        }
        else if (strcasecmp(token, "next_letter") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_NEXT_LETTER;
        }
        else if (strcasecmp(token, "add_letter") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_ADD_LETTER;
        }
        else if (strcasecmp(token, "remove_letter") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_REM_LETTER;
        }
        else if (strcasecmp(token, "prev_word") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_PREV_WORD;
        }
        else if (strcasecmp(token, "next_word") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_NEXT_WORD;
        }
        else if (strcasecmp(token, "lower_case") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_LOWER_CASE;
        }
        else if (strcasecmp(token, "upper_case") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_UPPER_CASE;
        }
        else if (strcasecmp(token, "toggle_case") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_TOGGLE_CASE;
        }
        else if (strcasecmp(token, "finish_text") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_ACCEPT_INPUT;
        }
        else if (strcasecmp(token, "cancel_text") == 0)
        {
            // Can't set mouse_slow to the special buttons
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            config->button[btn].action  = ACT_SPECIAL;
            config->button[btn].special = SPC_CANCEL_INPUT;
        }
        else if (strcasecmp(token, "hold_state") == 0)
        {
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            token = tokens_next(token_state);
            if (token == NULL)
            {
                return;
            }

            set_btn_as_mouse(btn, config, MOUSE_MOVEMENT_OFF);
            config->button[btn].action = ACT_STATE_HOLD;
            config->button[btn].cfg_name = string_register(token);
            config->map_check = true;
        }
        else if (strcasecmp(token, "push_state") == 0)
        {
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            token = tokens_next(token_state);
            if (token == NULL)
            {
                return;
            }

            set_btn_as_mouse(btn, config, MOUSE_MOVEMENT_OFF);
            config->button[btn].action = ACT_STATE_PUSH;
            config->button[btn].cfg_name = string_register(token);
            config->map_check = true;
        }
        else if (strcasecmp(token, "set_state") == 0)
        {
            token = tokens_next(token_state);
            if (token == NULL)
            {
                fprintf(stderr, "set_state without a state specified on %s.\n", gbtn_names[btn]);
                return;
            }

            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                return;
            }

            set_btn_as_mouse(btn, config, MOUSE_MOVEMENT_OFF);
            config->button[btn].action = ACT_STATE_PUSH;
            config->button[btn].cfg_name = string_register(token);
            config->map_check = true;
        }
        else if (strcasecmp(token, "pop_state") == 0)
        {
            if (btn >= GBTN_MAX)
            {
                fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                tokens_free(token_state);
                return;
            }

            set_btn_as_mouse(btn, config, MOUSE_MOVEMENT_OFF);
            config->button[btn].action = ACT_STATE_POP;
        }
        else if ((strcasecmp(token, "add_alt") == 0) || (!first_run && (strcasecmp(token, "alt") == 0)))
        {
            if (btn >= GBTN_MAX)
            {
                for (int sbtn=special_button_min(btn); sbtn < special_button_max(btn); sbtn++)
                    config->button[sbtn].modifier |= MOD_ALT;
            }
            else
            {
                config->button[btn].modifier |= MOD_ALT;
            }
        }
        else if ((strcasecmp(token, "add_ctrl") == 0) || (!first_run && (strcasecmp(token, "ctrl") == 0)))
        {
            if (btn >= GBTN_MAX)
            {
                for (int sbtn=special_button_min(btn); sbtn < special_button_max(btn); sbtn++)
                    config->button[sbtn].modifier |= MOD_CTRL;
            }
            else
            {
                config->button[btn].modifier |= MOD_CTRL;
            }
        }
        else if ((strcasecmp(token, "add_shift") == 0) || (!first_run && (strcasecmp(token, "shift") == 0)))
        {
            if (btn >= GBTN_MAX)
            {
                for (int sbtn=special_button_min(btn); sbtn < special_button_max(btn); sbtn++)
                    config->button[sbtn].modifier |= MOD_SHIFT;
            }
            else
            {
                config->button[btn].modifier |= MOD_SHIFT;
            }
        }
        else if (strcasecmp(token, "repeat") == 0)
        {
            if (btn >= GBTN_MAX)
            {
                for (int sbtn=special_button_min(btn); sbtn < special_button_max(btn); sbtn++)
                {
                    config->button[sbtn].repeat = true;
                }
            }
            else
            {
                config->button[btn].repeat = true;
            }
        }
        else if (strcasecmp(token, "parent") == 0)
        {
            set_btn_as_mouse(btn, config, MOUSE_MOVEMENT_PARENT);
            if (btn >= GBTN_MAX)
            {
                for (int sbtn=special_button_min(btn); sbtn < special_button_max(btn); sbtn++)
                {
                    config->button[sbtn].keycode  = 0;
                    config->button[sbtn].modifier = 0;
                    config->button[sbtn].action   = ACT_PARENT;
                    config->button[sbtn].special  = SPC_NONE;
                }
            }
            else
            {
                config->button[btn].keycode  = 0;
                config->button[btn].modifier = 0;
                config->button[btn].action   = ACT_PARENT;
                config->button[btn].special  = SPC_NONE;
            }
        }
        else if (strcasecmp(token, "clear") == 0)
        {
            set_btn_as_mouse(btn, config, MOUSE_MOVEMENT_OFF);
            if (btn >= GBTN_MAX)
            {
                for (int sbtn=special_button_min(btn); sbtn < special_button_max(btn); sbtn++)
                {
                    config->button[sbtn].keycode  = 0;
                    config->button[sbtn].modifier = 0;
                    config->button[sbtn].action   = ACT_NONE;
                    config->button[sbtn].special  = SPC_NONE;
                }
            }
            else
            {
                config->button[btn].keycode  = 0;
                config->button[btn].modifier = 0;
                config->button[btn].action   = ACT_NONE;
                config->button[btn].special  = SPC_NONE;
            }
        }
        else
        {
            const keyboard_values *key = find_keyboard(token);

            if (key != NULL)
            {
                set_btn_as_mouse(btn, config, MOUSE_MOVEMENT_OFF);

                if (btn >= GBTN_MAX)
                {
                    for (int sbtn=special_button_min(btn); sbtn < special_button_max(btn); sbtn++)
                    {
                        config->button[sbtn].keycode = key->keycode;
                        config->button[sbtn].action  = ACT_NONE;
                        config->button[sbtn].special = SPC_NONE;
                    }
                }
                else
                {
                    config->button[btn].keycode  = key->keycode;
                    config->button[btn].action   = ACT_NONE;
                    config->button[btn].special  = SPC_NONE;
                    // CEBION SAID NO
                    // config->button[btn].modifier |= key->modifier;
                }
            }
            else if (strcasecmp(token, "mouse_movement") == 0)
            {
                if (btn >= GBTN_MAX)
                {
                    set_btn_as_mouse(btn, config, MOUSE_MOVEMENT_ON);

                    for (int sbtn=special_button_min(btn), i=0; sbtn < special_button_max(btn); sbtn++, i++)
                    {
                        config->button[sbtn].keycode = 0;
                        config->button[sbtn].action  = ACT_NONE;
                        config->button[sbtn].special = SPC_NONE;
                    }
                }
                else
                {
                    fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                    return;
                }
            }
            else if (strcasecmp(token, "arrow_keys") == 0)
            {
                if (btn >= GBTN_MAX)
                {
                    set_btn_as_mouse(btn, config, MOUSE_MOVEMENT_OFF);

                    short keycodes[] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT};
                    for (int sbtn=special_button_min(btn), i=0; sbtn < special_button_max(btn); sbtn++, i++)
                    {
                        config->button[sbtn].keycode = keycodes[i];
                        config->button[sbtn].action  = ACT_NONE;
                        config->button[sbtn].special = SPC_NONE;
                    }
                }
                else
                {
                    fprintf(stderr, "error: unable to set %s to %s\n", token, gbtn_names[btn]);
                    return;
                }
            }
            else if (strcmp(token, "\\\"") == 0)
            {
                // GPTK2_DEBUG("# empty key %s, %s = %s\n", token, name, value);
                // DO NOTHING
            }
            else
            {
                fprintf(stderr, "warning: unknown key \"%s\" in binding: %s = \"%s\"\n", token, name, value);
                // GPTK2_DEBUG("# unknown key \"%s\", %s = \"%s\"\n", token, name, value);
            }
        }

        token = tokens_next(token_state);
        first_run = false;
    }
}


static int config_ini_handler(
    void* user, const char* section, const char* name, const char* value)
{
    config_parser* config = (config_parser*)user;

    char *temp_buffer = tabulate_text(value);

    if (temp_buffer == NULL)
        return 0;

    token_ctx *token_state = tokens_create(temp_buffer, '\t');
    free(temp_buffer);

    const char *token = tokens_next(token_state);

    if (strcmp(config->last_section, section) != 0)
    {
        // GPTK2_DEBUG("%s:\n", section);
        strncpy(config->last_section, section, MAX_CONTROL_NAME-1);

        if (strcasecmp(section, "config") == 0)
        {
            GPTK2_DEBUG("CONFIG\n");
            config->state = CFG_CONFIG;
        }
        else if (strlen(game_prefix) > 0 && strcasestartswith(section, "config:") == true)
        {
            const char *section_gameprefix = section + 7; // strlen("config:")

            // GPTK2_DEBUG("CONFIG++: %s\n", section_gameprefix);
            if (strcasecmp(section_gameprefix, game_prefix) == 0)
            {
                config->state = CFG_CONFIG;
                // GPTK2_DEBUG("ACCEPT GAME OVERRIDE \"%s\": %s = %s\n", section, name, value);
            }
            else
            {
                // GPTK2_DEBUG("IGNORE GAME OVERRIDE \"%s\": %s = %s\n", section, name, value);
                tokens_free(token_state);
                return 1;
            }
        }
        else if (strcasecmp(section, "controls") == 0)
        {
            // GPTK2_DEBUG("CONTROLS\n");
            config->state = CFG_CONTROL;
            config->current_config = root_config;

            gptk_hk_can_fix = false;
        }
        else if (strcasestartswith(section, "controls:") == true)
        {
            // GPTK2_DEBUG("CONTROLS++: %s\n", section);
            config->state = CFG_CONTROL;
            config->current_config = config_create(section);

            gptk_hk_can_fix = false;
        }
        else
        {
            // GPTK2_DEBUG("OTHER %s\n", section);
            config->state = CFG_OTHER;
        }
    }

    if (config->state == CFG_GPTK)
    {   // GPTK mode. :(
        const button_match *button = find_button(name);

        if (button != NULL)
        {   // fixes things from old gptk files.
            if (strcasestartswith(token, "mouse_movement_"))
            {
                if (GBTN_IS_DPAD(button->gbtn))
                {
                    name = "dpad";
                    token = "mouse_movement";
                    button = find_button(name);
                }
                else if (GBTN_IS_LEFT_ANALOG(button->gbtn))
                {
                    name = "left_analog";
                    token = "mouse_movement";
                    button = find_button(name);
                }
                else if (GBTN_IS_RIGHT_ANALOG(button->gbtn))
                {
                    name = "right_analog";
                    token = "mouse_movement";
                    button = find_button(name);
                }
            }

            set_btn_config(config->current_config, button->gbtn, name, token, token_state);
            // GPTK2_DEBUG("G: %s: %s, (%s, %d)\n", name, value, button->str, button->gbtn);
        }
        else if (strcasecmp(name, "overlay") == 0)
        {
            if (strcasecmp(token, "parent") == 0)
            {
                // GPTK2_DEBUG("overlay = parent\n");
                config_overlay_parent(config->current_config);
            }
            else if (strcasecmp(token, "clear") == 0)
            {
                // GPTK2_DEBUG("overlay = clear\n");
                config_overlay_clear(config->current_config);
            }
            else if (strlen(token) > 0)
            {
                // GPTK2_DEBUG("overlay = %s\n", value);
                config_overlay_named(config->current_config, token);
            }
            else
            {
                // fprintf(stderr, "overlay = (blank)\n");
            }
        }
        else if (strcaseendswith(name, "_hk"))
        {
            char *temp = (char*)gptk_malloc(GPTK_HK_FIX_MAX_LINE);

            snprintf(temp, GPTK_HK_FIX_MAX_LINE, "%s=%s", name, value);

            gptk_hk_fix_text[gptk_hk_fix_offset++] = temp;
        }
        else
        {
            set_cfg_config(name, token, token_state);
            // GPTK2_DEBUG("G: %s: %s\n", name, value);
        }
    }

    else if (config->state == CFG_CONFIG)
    {   // config mode.
        set_cfg_config(name, token, token_state);
        // GPTK2_DEBUG("C: %s: %s\n", name, value);
    }

    else if (config->state == CFG_CONTROL)
    {   // controls mode.
        const button_match *button = find_button(name);

        if (button != NULL)
        {
            set_btn_config(config->current_config, button->gbtn, name, token, token_state);
            // GPTK2_DEBUG("X: %s: %s (%s, %d)\n", name, value, button->str, button->gbtn);
        }
        else if (strcasecmp(name, "overlay") == 0)
        {
            if (strcasecmp(token, "parent") == 0)
            {
                // GPTK2_DEBUG("overlay = parent\n");
                config_overlay_parent(config->current_config);
            }
            else if (strcasecmp(token, "clear") == 0)
            {
                // GPTK2_DEBUG("overlay = clear\n");
                config_overlay_clear(config->current_config);
            }
            else if (strlen(token) > 0)
            {
                // GPTK2_DEBUG("overlay = %s\n", value);
                config_overlay_named(config->current_config, token);
            }
            else
            {
                // fprintf(stderr, "overlay = (blank)\n");
            }
        }
        else if (strcasecmp(name, "charset") == 0)
        {
            const char_set *cfg_charset = find_char_set(token);
            if (cfg_charset == NULL)
            {
                fprintf(stderr, "charset unable to find \"%s\" charset\n", token);
            }
            else
            {
                config->current_config->charset = string_register(token);
                config->current_config->wordset = NULL;
            }
        }
        else if (strcasecmp(name, "wordset") == 0)
        {
            const word_set *cfg_wordset = find_word_set(token);

            if (cfg_wordset == NULL)
            {
                fprintf(stderr, "wordset unable to find \"%s\" wordset\n", token);
            }
            else
            {
                config->current_config->wordset = string_register(token);
                config->current_config->charset = NULL;
            }
        }
        else if (strcasecmp(name, "exclusive") == 0)
        {
            if (strcasecmp(token, "parent") == 0)
                config->current_config->exclusive_mode = EXL_PARENT;

            else if (strcasecmp(token, "true") == 0)
                config->current_config->exclusive_mode = EXL_TRUE;

            else
                config->current_config->exclusive_mode = EXL_FALSE;
        }
        else
        {
            GPTK2_DEBUG("# unknown config %s = %s\n", name, value);
        }
    }
    else
    {
        GPTK2_DEBUG("?: %s: %s\n", name, value);
    }

    tokens_free(token_state);
    return 1;
}


int config_load(const char *file_name, bool config_only)
{
    config_parser config;

    memset((void*)&config, '\0', sizeof(config_parser));

    config.state = CFG_GPTK;
    config.current_config = root_config;
    config.config_only = config_only;

    if (config_only)
        config.state = CFG_CONFIG;

    if (ini_parse(file_name, config_ini_handler, &config) < 0)
    {
        printf("Can't load '%s'\n", file_name);
        return 1;
    }

    return 0;
}


void config_finalise()
{   // this will check all the configs loaded and link the cfg_name to cfg_maps
    gptokeyb_config *current = root_config;
    char* env_textinput = SDL_getenv("TEXTINPUTINTERACTIVE");

    config_parser config;

    memset((void*)&config, '\0', sizeof(config_parser));

    config.state = CFG_GPTK;
    config.current_config = root_config;
    config.config_only = false;

    if (gptk_hk_can_fix)
    {   // if we have only seen a gptk file we can convert <key>_hk automatically.
        current->overlay_mode = OVL_CLEAR;

        if (gptk_hk_fix_offset > 0)
        {
            config_ini_handler((void*)&config, "controls", "hotkey", "hold_state hk_hotkey");

            config_ini_handler((void*)&config, "controls:hk_hotkey", "overlay", "parent");

            for (int i=0; i < gptk_hk_fix_offset; i++)
            {
                char *name = gptk_hk_fix_text[i];
                char *value = strchr(gptk_hk_fix_text[i], '=');
                value++;

                // printf("REGESTERING: '%s' = '%s'", name, value);

                if (value == NULL || strlen(value) == 0)
                {
                    // printf(": bad value\n");
                    continue;
                }
                // printf("\n");

                *(value - 4) = '\0';

                config_ini_handler((void*)&config, "controls:hk_hotkey", name, value);
            }
        }

        if (env_textinput != NULL && atob_default(env_textinput, true) == true)
        {   // if we have only seen a gptk file we enable the old text_input mode.
            config_ini_handler((void*)&config, "controls", "start", "hold_state hk_start");
            config_ini_handler((void*)&config, "controls:hk_start", "overlay", "parent");
            config_ini_handler((void*)&config, "controls:hk_start", "down", "push_state text_input");

            config_ini_handler((void*)&config, "controls:text_input", "overlay", "clear");
            config_ini_handler((void*)&config, "controls:text_input", "charset", "full");

            const struct { char *button; char *keybind; } text_input[] = {
                {"up",   "prev_letter"},
                {"down", "next_letter"},

                {"right", "add_letter"},
                {"left",  "remove_letter"},

                {"a",     "add_letter"},
                {"b",     "remove_letter"},

                {"start", "finish_text"},
                {"back",  "cancel_text"},
            };

            for (size_t i=0; i < (sizeof(text_input) / sizeof(text_input[0])); i++)
            {
                config_ini_handler((void*)&config, "controls:text_input", text_input[i].button, text_input[i].keybind);
            }
        }
    }

    while (current != NULL)
    {
        // GPTK2_DEBUG("Checking %s\n", current->name);

        if (current->map_check)
        {
            for (int btn=0; btn < GBTN_MAX; btn++)
            {
                // FUCK IT ALL
                if ((current->dpad_as_mouse && GBTN_IS_DPAD(btn)) ||
                    (current->left_analog_as_mouse  && GBTN_IS_LEFT_ANALOG(btn)) ||
                    (current->right_analog_as_mouse && GBTN_IS_RIGHT_ANALOG(btn)))
                {
                    current->button[btn].keycode = 0;
                    current->button[btn].action  = ACT_NONE;
                    current->button[btn].special = SPC_NONE;
                }

                if (current->button[btn].action >= ACT_STATE_HOLD)
                {
                    current->button[btn].cfg_map = config_find(current->button[btn].cfg_name);

                    if (current->button[btn].cfg_map == current)
                    {
                        fprintf(stderr, "%s: \"%s = %s %s\" is linking to the same map, clearing action.\n",
                            current->name,
                            gbtn_names[btn],
                            act_names[current->button[btn].action],
                            current->button[btn].cfg_name);

                        current->button[btn].cfg_map = NULL;
                        current->button[btn].action = ACT_NONE;
                    }
                    else if (current->button[btn].cfg_map == NULL)
                    {
                        fprintf(stderr, "%s: \"%s = %s %s\" is an unknown map, clearing action.\n",
                            current->name,
                            gbtn_names[btn],
                            act_names[current->button[btn].action],
                            current->button[btn].cfg_name);

                        current->button[btn].action = ACT_NONE;
                    }
                }
            }

            current->map_check = false;
        }

        current = current->next;
    }
}
