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

typedef struct _string_reg
{
    struct _string_reg *next;
    int counter;
    char *string;
} string_reg;

static string_reg *root_string = NULL;


void *gptk_malloc(size_t size)
{
    void *data = malloc(size);
    if (data == NULL)
    {
        fprintf(stderr, "Unable to allocate memory. :(\n");
        exit(255);
    }

    memset(data, 0, size);
    return data;
}


void *gptk_realloc(void *old_data, size_t old_size, size_t size)
{
    void *data = realloc(old_data, size);

    if (data == NULL)
    {
        fprintf(stderr, "Unable to reallocate memory. :(\n");
        exit(255);
    }

    if (size > old_size)
    {
        memset((char *)data + old_size, 0, size - old_size);
    }

    return data;
}


struct _token_ctx
{
    char separator;
    char *next_token;
    char *curr_token;
    size_t total_size;
    char full_buffer[];
};


token_ctx *tokens_create(const char *input_text, char separator)
{
    size_t input_len = strlen(input_text) + 1; // +1 for null terminator
    size_t total_size = sizeof(token_ctx) + input_len; // Total size including struct and input text
    token_ctx *token_state = (token_ctx *)gptk_malloc(total_size);

    token_state->separator = separator;
    token_state->total_size = total_size;
    strcpy(token_state->full_buffer, input_text);
    token_state->curr_token = NULL;
    token_state->next_token = token_state->full_buffer;

    return token_state;
}


void tokens_free(token_ctx *token_state)
{
    free(token_state);
}


const char *tokens_next(token_ctx *token_state)
{
    if (token_state->next_token == NULL)
        return NULL;

    if (token_state->curr_token != NULL)
        *(token_state->next_token-1) = token_state->separator;

    token_state->curr_token = token_state->next_token;

    // Find the next separator
    token_state->next_token = strchr(token_state->next_token, token_state->separator);
    if (token_state->next_token != NULL)
        *(token_state->next_token++) = '\0';

    return token_state->curr_token;
}


const char *tokens_prev(token_ctx *token_state)
{
    if (token_state->curr_token == NULL)
        return NULL;

    if (token_state->next_token != NULL)
        *(token_state->next_token-1) = token_state->separator;

    if (token_state->curr_token == token_state->full_buffer)
    {
        token_state->curr_token = NULL;
        token_state->next_token = token_state->full_buffer;
        return NULL;        
    }

    char *prev_separator_pos = token_state->curr_token - 2;
    while (prev_separator_pos >= token_state->full_buffer && *prev_separator_pos != token_state->separator)
        prev_separator_pos--;

    if (prev_separator_pos < token_state->full_buffer)
    {
        prev_separator_pos = token_state->full_buffer;
    }

    if (*prev_separator_pos == token_state->separator)
        prev_separator_pos++;

    token_state->curr_token = prev_separator_pos;
    token_state->next_token = strchr(token_state->curr_token, token_state->separator);
    if (token_state->next_token != NULL)
        *(token_state->next_token++) = '\0';

    return token_state->curr_token;
}


const char *tokens_curr(token_ctx *token_state)
{
    return token_state->curr_token;
}


const char *tokens_rest(token_ctx *token_state)
{
    return token_state->next_token;
}


char *tabulate_text(const char *text)
{  
    char *temp_buffer;
    int text_len = strlen(text);

    if (text_len == 0)
        return NULL;

    temp_buffer = (char*)gptk_malloc(text_len+1);

    int t=0;
    int i=0;

    while (i < text_len)
    {
        if (text[i] == '"' || text[i] == '\'')
        {
            char c = text[i];
            i += 1;

            while (text[i] != c && i < text_len)
                temp_buffer[t++] = text[i++];

            temp_buffer[t++] = '\t';
            i++;
        }
        else if (text[i] == ' ' || text[i] == '\t')
        {
            if (t > 0 && temp_buffer[t-1] != '\t')
                temp_buffer[t++] = '\t';
            i++;

            while ((text[i] == ' ' || text[i] == '\t') && i < text_len)
                i++;
        }
        else
        {
            temp_buffer[t++] = text[i++];

            while (text[i] != ' ' && text[i] != '\t' && i < text_len)
                temp_buffer[t++] = text[i++];
        }
    }
    temp_buffer[t] = '\0';

    return temp_buffer;
}


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


void emitModifier(bool pressed, int modifier)
{
    if ((modifier & MOD_SHIFT) != 0)
    {
        emit(EV_KEY, KEY_LEFTSHIFT, pressed ? 1 : 0);
        emit(EV_SYN, SYN_REPORT, 0);        
    }

    if ((modifier & MOD_ALT) != 0)
    {
        emit(EV_KEY, KEY_LEFTALT, pressed ? 1 : 0);
        emit(EV_SYN, SYN_REPORT, 0);        
    }

    if ((modifier & MOD_CTRL) != 0)
    {
        emit(EV_KEY, KEY_LEFTCTRL, pressed ? 1 : 0);
        emit(EV_SYN, SYN_REPORT, 0);        
    }
}


void emitKey(int code, bool pressed, int modifier)
{
    if (code == 0)
        return;

    if ((modifier != 0) && pressed)
        emitModifier(pressed, modifier);

    if (code == BTN_GEAR_UP)
    {
        if (pressed)
            emitMouseWheel(-current_mouse_wheel_amount);
    }
    else if (code == BTN_GEAR_DOWN)
    {
        if (pressed)
            emitMouseWheel(current_mouse_wheel_amount);
    }
    else
    {
        emit(EV_KEY, code, pressed ? 1 : 0);
        emit(EV_SYN, SYN_REPORT, 0);
    }

    if ((modifier != 0) && !(pressed))
        emitModifier(pressed, modifier);
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


void emitMouseWheel(int wheel)
{
    if (wheel != 0)
    {
        emit(EV_REL, REL_WHEEL, wheel);
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


#define KILL_BUFFER 1024

bool process_with_pkill(const char *process_name, bool use_sudo)
{
    char temp_buffer[KILL_BUFFER];

    if (use_sudo)
        snprintf(temp_buffer, KILL_BUFFER, "sudo pkill -9 '%s'", process_name);
    else
        snprintf(temp_buffer, KILL_BUFFER, "pkill -9 '%s'", process_name);

    // Execute the command
    int status = system(temp_buffer);

    // Check if the command executed successfully
    if (status == -1)
        return false;

    return true;
}


bool process_with_kill(const char *process_name, bool use_sudo)
{
    char temp_buffer[KILL_BUFFER];
    bool status = false;

    snprintf(temp_buffer, KILL_BUFFER, "ps | grep '%s' | grep -v grep | awk '{print $1}'", process_name);

    FILE *fp = popen(temp_buffer, "r");
    if (fp == NULL) {
        perror("Error executing ps command");
        return false;
    }

    char pid_str[32];

    if (fgets(pid_str, sizeof(pid_str), fp) != NULL)
    {
        // Trim the newline character
        pid_str[strcspn(pid_str, "\n")] = 0;

        // Convert string to integer
        int pid = atoi(pid_str);

        // Kill the process
        if (use_sudo)
            snprintf(temp_buffer, KILL_BUFFER, "sudo kill -9 %d", pid);
        else
            snprintf(temp_buffer, KILL_BUFFER, "kill -9 %d", pid);

        int status = system(temp_buffer);
        if (status == -1)
        {
            perror("Error executing kill command");
        }
        else
        {
            status = true;
            printf("Process with name '%s' and PID %d killed successfully.\n", process_name, pid);
        }
    }
    else
    {
        printf("No process with name '%s' found.\n", process_name);
    }

    pclose(fp);

    return status;
}


void process_with_pc_quit()
{
    emitKey(KEY_F4, true, KEY_LEFTALT);
    SDL_Delay(15);

    emitKey(KEY_F4, false, KEY_LEFTALT);
    SDL_Delay(15);
}


bool process_kill()
{
    if (want_pc_quit)
        process_with_pc_quit();

    if (strlen(kill_process_name) == 0)
        return false;

    if (want_kill)
        return process_with_kill(kill_process_name, want_sudo);

    return process_with_pkill(kill_process_name, want_sudo);
}


string_reg *string_reg_create(const char *string)
{
    if (string == NULL)
        return NULL;

    string_reg *current_string = (string_reg *)gptk_malloc(sizeof(string_reg));
    size_t string_len = strlen(string);

    current_string->string = (char *)gptk_malloc(string_len + 1);

    strncpy(current_string->string, string, strlen(string));

    return current_string;
}

void string_init()
{
    root_string = string_reg_create("controls");
}

void string_quit()
{
    string_reg *current_string = root_string;
    string_reg *next_string;
    int total_strings = 0;
    int total_lookups = 0;
    int lookup_factor = 0;

    // printf("# String Quit\n");
    int i=1;
    while (current_string != NULL)
    {
        next_string = current_string->next;

        total_strings++;
        total_lookups += current_string->counter;
        lookup_factor += i * current_string->counter;

        // printf("# - %s\n", current_string->string);
        free(current_string->string);
        free(current_string);

        current_string = next_string;
        i++;
    }

    // printf("# Stats: %d, %d, %d\n", total_strings, total_lookups, lookup_factor);
}

const char *string_register(const char *string)
{
    if (string == NULL)
        return NULL;

    string_reg *current_string = root_string;
    string_reg *last_string;

    while (current_string != NULL)
    {
        last_string = current_string;

        if (strcmp(current_string->string, string) == 0)
        {
            current_string->counter++;
            return current_string->string;
        }

        current_string = current_string->next;
    }

    current_string = string_reg_create(string);
    current_string->counter++;
    last_string->next = current_string;

    return current_string->string;
}
