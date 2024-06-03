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


/*

[config]
wordset = "level_select" "01" "02" "03" "04" "05" "06" "07" "08" "09" "10"

[controls:text_input]
overlay = clear

charset = "basic"

up    = prev_letter
down  = next_letter

right = add_letter
left  = remove_letter

a     = add_letter
b     = remove_letter

l1    = prev_letter10
r1    = next_letter10

start = finish_text
back  = cancel_text

[constrols:level_select]
wordset = "level_select"

up    = next_word
down  = prev_word

a = finish_text
*/


typedef struct {
    short keycode;
    bool  shift;
} character_map;


#define MAX_INPUT_LENGTH 128
#define MAX_TEXT_LENGTH  64
#define WORDS_SIZE_DEFAULT 16

bool char_set_builtin_mode = true;
char_set *root_char_set=NULL;
word_set *root_word_set=NULL;

character_map characters[256];

const char_set *active_char_set;
const word_set *active_word_set;

#define INPUT_MAGIC_CHAR 0xFFFF

static char input_text[MAX_TEXT_LENGTH+1] = {0};
static int  input_buffer[MAX_TEXT_LENGTH+1] = {0};

static int current_letter = 0;
static size_t current_offset = 0;

static int current_word = 0;
static size_t current_word_len = 0;


void disable_input()
{
    active_char_set = NULL;
    active_word_set = NULL;
}


bool input_active()
{
    return (active_char_set != NULL || active_word_set != NULL);
}


char_set *_find_char_set(const char *name)
{
    char_set *curr_char_set = root_char_set;

    while (curr_char_set != NULL)
    {
        if (strcasecmp(name, curr_char_set->name) == 0)
        {
            return curr_char_set;
        }

        curr_char_set = curr_char_set->next;
    }

    return NULL;
}


const char_set *find_char_set(const char *name)
{
    char_set *curr_char_set = _find_char_set(name);

    if (curr_char_set == NULL)
        return root_char_set;

    return curr_char_set;
}


word_set *_find_word_set(const char *name)
{
    word_set *curr_word_set = root_word_set;
    word_set *prev_word_set = NULL;

    while (curr_word_set != NULL)
    {
        if (strcasecmp(name, curr_word_set->name) == 0)
        {
            if (prev_word_set != NULL)
            {   // unlink it
                prev_word_set->next = curr_word_set->next;
                curr_word_set->next = NULL;
            }

            if (curr_word_set != root_word_set)
            {   // move current wordset to the head of the root set
                curr_word_set->next = root_word_set;
                root_word_set = curr_word_set;
            }

            return curr_word_set;
        }

        prev_word_set = curr_word_set;
        curr_word_set = curr_word_set->next;
    }

    return NULL;
}


const word_set *find_word_set(const char *name)
{
    return _find_word_set(name);
}


void input_init()
{
    // Tag these as builtin charsets, if you override them they will be tagged as custom.
    // This helps the dumping code.
    char_set_builtin_mode = true;

    register_char_set("basic",    "ABCDEFGHIJKLMNOPQRSTUVWXYZ .,-_() 0123456789 ");
    register_char_set("extended", "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz .,-_() 0123456789 ");
    register_char_set("full",     "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz .,-_()@#%&*-+!\"\':;/?~`|{}$^=""[]\\<> 0123456789");
    register_char_set("digits",   "0123456789");
    register_char_set("hex",      "0123456789ABCDEF");
    register_char_set("alpha",    "ABCDEFGHIJKLMNOPQRSTUVWXYZ");

    char_set_builtin_mode = false;

    const char_set *full_set = find_char_set("full");

    size_t character_len = strlen(full_set->characters);
    const keyboard_values *keyinfo;
    memset((void*)characters, '\0', sizeof(characters));
    char find_buff[3] = "\0\0\0";

    for (size_t i = 0; i < character_len; i++)
    {
        find_buff[0] = full_set->characters[i];
        keyinfo = find_keyboard(find_buff);

        if (keyinfo == NULL)
        {
            printf("bad character '%s'\n", keyinfo->str);
            continue;
        }

        characters[(unsigned char)full_set->characters[i]].keycode = keyinfo->keycode;
        characters[(unsigned char)full_set->characters[i]].shift   = (keyinfo->modifier & MOD_SHIFT) != 0;
    }

    for (int i=0; i < MAX_INPUT_LENGTH; i++)
        input_buffer[i] = INPUT_MAGIC_CHAR;
}


void dump_char_sets()
{
    char_set *curr_char_set = root_char_set;

    while (curr_char_set != NULL)
    {
        if (curr_char_set->builtin)
        {
            curr_char_set = curr_char_set->next;

            continue;
        }

        printf("charset = \"%s\"", curr_char_set->name);

        if (strchr(curr_char_set->characters, '"') == NULL)
            printf(" \"%s\"\n", curr_char_set->characters);

        else if (strchr(curr_char_set->characters, '\'') == NULL)
            printf(" '%s'\n", curr_char_set->characters);

        else
            printf(" \"%s\"\n", curr_char_set->characters);

        curr_char_set = curr_char_set->next;
    }
}


void dump_word_sets()
{
    word_set *curr_word_set = root_word_set;

    while (curr_word_set != NULL)
    {
        printf("wordset = \"%s\"", curr_word_set->name);

        for (size_t i=0; i < curr_word_set->words_len; i++)
        {
            if (strchr(curr_word_set->words[i], '"') == NULL)
                printf(" \"%s\"", curr_word_set->words[i]);

            else if (strchr(curr_word_set->words[i], '\'') == NULL)
                printf(" '%s'", curr_word_set->words[i]);

            else
                printf(" \"%s\"", curr_word_set->words[i]);
        }

        printf("\n");
        curr_word_set = curr_word_set->next;
    }
}


void register_char_set(const char *name, const char *characters)
{
    bool char_set_created = false;
    char_set *new_char_set = _find_char_set(name);

    if (new_char_set == NULL)
    {
        char_set_created = true;
        new_char_set = (char_set*)gptk_malloc(sizeof(char_set));
    }

    new_char_set->name       = string_register(name);
    new_char_set->characters = string_register(characters);
    new_char_set->characters_len = strlen(characters);
    new_char_set->builtin    = char_set_builtin_mode;

    if (char_set_created)
    {
        if (root_char_set != NULL)
        {
            char_set *curr_char_set = root_char_set;
            while (curr_char_set->next != NULL)
                curr_char_set = curr_char_set->next;

            curr_char_set->next = new_char_set;
        }
        else
        {
            root_char_set = new_char_set;
        }
    }
}


void register_word_set(const char *name, const char *word)
{
    word_set *curr_word_set = _find_word_set(name);

    if (curr_word_set == NULL)
    {   // create a new one
        curr_word_set = (word_set*)gptk_malloc(sizeof(word_set));

        curr_word_set->name = string_register(name);
        curr_word_set->words = (const char **)gptk_malloc(sizeof(char*) * WORDS_SIZE_DEFAULT);
        curr_word_set->words_alloc = WORDS_SIZE_DEFAULT;

        curr_word_set->next  = root_word_set;
        root_word_set = curr_word_set;
    }

    if ((curr_word_set->words_len+1) >= curr_word_set->words_alloc)
    {   // resize the words buffer if we need to
        size_t new_size = curr_word_set->words_alloc + WORDS_SIZE_DEFAULT;

        curr_word_set->words = (const char **)gptk_realloc(
            curr_word_set->words,
            sizeof(char*) * (curr_word_set->words_alloc),
            sizeof(char*) * new_size);

        curr_word_set->words_alloc = new_size;
    }

    // printf("register_word_set(%s, %s)\n", name, word);

    curr_word_set->words[curr_word_set->words_len++] = string_register(word);
}


void input_quit()
{
    char_set *curr_char_set = root_char_set;
    char_set *next_char_set;

    while (curr_char_set != NULL)
    {
        next_char_set = curr_char_set->next;

        free(curr_char_set);

        curr_char_set = next_char_set;
    }

    root_char_set = NULL;

    word_set *curr_word_set = root_word_set;
    word_set *next_word_set;

    while (curr_word_set != NULL)
    {
        next_word_set = curr_word_set->next;

        free(curr_word_set->words);
        free(curr_word_set);

        curr_word_set = next_word_set;
    }

    root_word_set = NULL;
}


void input_load_char_set(const char *name)
{   // load a char set, deactivate any word sets.
    if (!input_active())
    {   // Clear buffers, setup for new input session.
        for (int i=0; i < (MAX_INPUT_LENGTH+1); i++)
        {
            input_text[i] = 0;
            input_buffer[i] = INPUT_MAGIC_CHAR;
        }

        current_offset = 0;
        current_letter = 0;
        current_word = 0;
        current_word_len = 0;
    }

    active_char_set = find_char_set(name);

    if (active_char_set == NULL)
    {   // we always fallback to the basic charset.
        fprintf(stderr, "Unknown wordset \"%s\".\n", name);

        active_char_set = find_char_set("basic");

        if ((size_t)current_letter >= active_char_set->characters_len)
            current_letter = 0;
    }
    else
    {
        printf("Loaded char set \"%s\"\n", name);
    }

    // Clear the active word set
    active_word_set = NULL;
}


void input_load_word_set(const char *name)
{   // load a word set, deactivate any char sets.
    if (!input_active())
    {   // Clear buffers, setup for new input session.
        for (int i=0; i < (MAX_INPUT_LENGTH+1); i++)
        {
            input_text[i] = 0;
            input_buffer[i] = INPUT_MAGIC_CHAR;
        }

        current_offset = 0;
        current_letter = 0;
        current_word = 0;
        current_word_len = 0;
    }

    const word_set *temp_wordset = find_word_set(name);;

    if (active_word_set == NULL)
    {
        fprintf(stderr, "Unknown wordset \"%s\".\n", name);
        return;
    }

    printf("Loaded word set \"%s\"\n", name);
    active_word_set = temp_wordset;
    active_char_set = NULL;

    if ((size_t)current_word >= active_word_set->words_len)
        current_word = 0;
}


void input_stop()
{
    if (active_word_set != NULL || active_char_set != NULL)
        printf("Cleared input sets\n");

    active_word_set = NULL;
    active_char_set = NULL;
}


void input_rem_char()
{
    // printf("input_rem_char\n");
    printf("input_rem_char\n");

    emitTextInputKey(KEY_BACKSPACE, false);
}


void input_add_char()
{
    // printf("input_add_char -- %s\n", input_text);
    emitTextInputKey(
        characters[(unsigned char)input_text[current_offset]].keycode,
        characters[(unsigned char)input_text[current_offset]].shift);
}


void input_clear_state()
{
    printf("input_clear_state\n");

    while (current_offset > 0)
    {
        input_rem_letter();
    }
}


void input_accept()
{
    printf("input_accept\n");

    emitTextInputKey(KEY_ENTER, false);
    pop_state();
}


void input_cancel()
{
    printf("input_cancel\n");

    input_clear_state();
    emitTextInputKey(KEY_ENTER, false);
    pop_state();
}


void input_set_state(const char *buff, size_t buff_len)
{
    if (buff_len >= MAX_TEXT_LENGTH)
    {
        buff_len = MAX_TEXT_LENGTH - 1; // Ensure we do not exceed buffer length
    }

    current_offset = 0;
    for (size_t i=0; i < buff_len; i++)
    {
        input_text[i] = buff[i];
        input_buffer[i] = INPUT_MAGIC_CHAR;
    }

    input_text[buff_len] = 0;
    input_buffer[buff_len] = INPUT_MAGIC_CHAR;

    while (current_offset < buff_len)
        input_add_letter();
}


void input_get_state(char *buff, size_t buff_len)
{
    if (buff_len > 0)
    {
        if (buff_len > current_offset + 1)
        {
            buff_len = current_offset + 1;
        }

        strncpy(buff, input_text, buff_len - 1);

        buff[buff_len - 1] = '\0';
    }
}


void input_add_letter()
{
    printf("input_add_letter\n");

    if (current_offset < MAX_TEXT_LENGTH && active_char_set)
    {
        current_offset++;

        // if input_buffer[current_offset] is INPUT_MAGIC_CHAR, set it to the current letter
        if (input_buffer[current_offset] == INPUT_MAGIC_CHAR)
        {
            if (input_buffer[current_offset] != 0)
            {
                char *temp = strchr(active_char_set->characters, input_text[current_offset]);
                if (temp != NULL)
                    current_letter = (int)(active_char_set->characters - temp);
            }

            input_buffer[current_offset] = current_letter;
            input_text[current_offset] = active_char_set->characters[current_letter];
        }
        else
        {
            current_letter = input_buffer[current_offset];
            input_text[current_offset] = active_char_set->characters[current_letter];
        }

        input_add_char();
    }
}


void input_rem_letter()
{
    printf("input_rem_letter\n");

    if (current_offset > 0)
    {
        input_rem_char();

        input_text[current_offset] = 0;
        current_offset--;
    }
}


void input_next_letter(int amount)
{
    printf("input_next_letter(%d)\n", amount);
    if (active_char_set)
    {
        current_letter = (current_letter + amount) % active_char_set->characters_len;

        if (current_letter < 0)
            current_letter += active_char_set->characters_len;

        input_rem_char();
        input_buffer[current_offset] = current_letter;
        input_text[current_offset] = active_char_set->characters[current_letter];
        input_add_char();
    }
}


void input_prev_letter(int amount)
{
    printf("input_prev_letter(%d)\n", amount);
    if (active_char_set)
    {
        current_letter = (current_letter - amount) % active_char_set->characters_len;

        if (current_letter < 0)
            current_letter += active_char_set->characters_len;

        input_rem_char();
        input_buffer[current_offset] = current_letter;
        input_text[current_offset] = active_char_set->characters[current_letter];
        input_add_char();
    }
}


void input_next_word(int amount)
{
    if (active_word_set)
    {
        current_word = (current_word + amount) % active_word_set->words_len;

        if (current_word < 0)
            current_word += active_word_set->words_len;

        input_clear_state();
        input_set_state(
            active_word_set->words[current_word],
            strlen(active_word_set->words[current_word]));
    }
}

void input_prev_word(int amount)
{
    if (active_word_set)
    {
        current_word = (current_word - amount) % active_word_set->words_len;

        if (current_word < 0)
            current_word += active_word_set->words_len;

        input_clear_state();
        input_set_state(
            active_word_set->words[current_word],
            strlen(active_word_set->words[current_word]));
    }
}

