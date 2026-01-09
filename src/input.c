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

static char input_text[MAX_TEXT_LENGTH] = {0};

static int current_letter = 0;
static size_t current_offset = 0;

static int current_word = 0;
static size_t current_word_len = 0;


void input_rem_char();
void input_add_char();
void input_dump_state();


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
            printf("bad character '%c'\n", full_set->characters[i]);
            continue;
        }

        characters[(unsigned char)full_set->characters[i]].keycode = keyinfo->keycode;
        characters[(unsigned char)full_set->characters[i]].shift   = (keyinfo->modifier & MOD_SHIFT) != 0;
    }

    for (int i=0; i < MAX_TEXT_LENGTH; i++)
        input_text[i] = 0;
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
    bool do_init = false;

    if (!input_active())
    {   // Clear buffers, setup for new input session.
        do_init = true;

        for (int i=0; i < MAX_TEXT_LENGTH; i++)
            input_text[i] = 0;

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
    }
    else
    {
        printf("Loaded char set \"%s\"\n", name);
    }

    if (input_text[current_offset] == 0)
    {
        current_letter = 0;
        input_text[current_offset] = active_char_set->characters[current_letter];

        if (!do_init)
            input_rem_char();

        input_add_char();
    }
    else
    {
        char *temp = strchr(active_char_set->characters, input_text[current_offset]);

        if (temp == NULL)
            current_letter = 0;
        else
            current_letter = (int)(temp - active_char_set->characters);

        input_text[current_offset] = active_char_set->characters[current_letter];

        if (!do_init)
            input_rem_char();

        input_add_char();
    }

    // Clear the active word set
    active_word_set = NULL;
}


void input_load_word_set(const char *name)
{   // load a word set, deactivate any char sets.
    if (!input_active())
    {   // Clear buffers, setup for new input session.
        for (int i=0; i < MAX_TEXT_LENGTH; i++)
            input_text[i] = 0;

        current_offset = 0;
        current_letter = 0;
        current_word = 0;
        current_word_len = 0;
    }

    const word_set *temp_wordset = find_word_set(name);;

    if (temp_wordset == NULL)
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
    unsigned char c = (unsigned char)input_text[current_offset];
    short keycode = characters[c].keycode;
    bool shift    = characters[c].shift;

    // Debug: show what character is being typed and how
    printf("input_add_char: '%c' (0x%02X) -> keycode=%d shift=%d\n",
           (c >= 32 && c < 127) ? c : '?', c,
           keycode ? keycode : characters['?'].keycode,
           shift ? 1 : 0);

    if (keycode == 0)
    {
        // Fallback to '?' if the character is unknown
        emitTextInputKey(
            characters['?'].keycode,
            characters['?'].shift);
    }
    else
    {
        emitTextInputKey(
            keycode,
            shift);
    }
}

void input_dump_state()
{
    printf("\n");
    printf("input_text -- %s\n", input_text);
    printf("              ");

    for (size_t i=0; i < current_offset; i++)
    {
        printf(" ");
    }

    printf("^\n");
}


bool _input_add_letter(char letter)
{
    // Leave room for trailing '\0'
    if (current_offset >= MAX_TEXT_LENGTH - 1)
        return false;

    // 1. Write letter at current position
    input_text[current_offset] = letter;

    // 2. Emit that character
    input_add_char();

    // 3. Move cursor to "after last char"
    current_offset++;

    // 4. Maintain proper C-string termination
    input_text[current_offset] = 0;

    return true;
}


void _input_rem_letter()
{
    if (current_offset == 0)
        return;

    current_offset--;
    input_text[current_offset] = 0;

    input_rem_char();
}

void input_rem_word()
{
    while (current_word_len > 0)
    {
        _input_rem_letter();
        current_word_len -= 1;
    }
}


void input_add_word(const char *word, size_t word_len)
{
    for (size_t i=0; i < word_len; i++)
    {
        if (!_input_add_letter(word[i]))
            break;

        current_word_len += 1;
    }
}


void input_clear_state()
{
    printf("input_clear_state\n");

    while (current_offset > 0)
    {
        _input_rem_letter();
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
        _input_add_letter(buff[i]);
    }
}


void input_get_state(char *buff, size_t buff_len)
{
    if (buff_len > 0)
    {
        if (buff_len > current_offset + 1)
        {
            buff_len = current_offset + 1;
        }

        strncpy(buff, input_text, buff_len);
    }
}


void input_add_letter()
{
    printf("input_add_letter\n");

    if (active_char_set != NULL && current_offset < MAX_TEXT_LENGTH)
    {
        current_offset++;

        if (input_text[current_offset] != 0)
        {
            char *temp = strchr(active_char_set->characters, input_text[current_offset]);

            if (temp != NULL)
                current_letter = (int)(temp - active_char_set->characters);
            else
                current_letter = 0;
        }

        input_text[current_offset] = active_char_set->characters[current_letter];

        input_add_char();

        input_dump_state();
    }
}


void input_lower_case()
{
    printf("lower_case ");
    if (active_char_set != NULL && isupper(input_text[current_offset]))
    {
        char lower = tolower(input_text[current_offset]);

        char *temp = strchr(active_char_set->characters, lower);

        if (temp != NULL)
        {
            current_letter = (int)(temp - active_char_set->characters);
            input_text[current_offset] = lower;

            input_rem_char();
            input_add_char();
        }

        printf("- lower\n");
    }
    else
        printf("- none\n");

    input_dump_state();
}


void input_upper_case()
{
    printf("upper_case ");

    if (active_char_set != NULL && islower(input_text[current_offset]))
    {
        char lower = toupper(input_text[current_offset]);

        char *temp = strchr(active_char_set->characters, lower);

        if (temp != NULL)
        {
            current_letter = (int)(temp - active_char_set->characters);
            input_text[current_offset] = lower;

            input_rem_char();
            input_add_char();
        }

        printf("- lower\n");
    }
    else
        printf("- none\n");

    input_dump_state();
}


void input_toggle_case()
{
    printf("toggle_case - ");

    if (islower(input_text[current_offset]))
        input_upper_case();

    else if (isupper(input_text[current_offset]))
        input_lower_case();

    else
        printf("none\n");
}


void input_rem_letter()
{
    printf("input_rem_letter");

    if (current_offset > 0)
    {
        char *temp;

        input_rem_char();

        current_offset--;

        if (active_char_set != NULL && input_text[current_offset] != 0)
        {
            temp = strchr(active_char_set->characters, input_text[current_offset]);

            if (temp != NULL)
                current_letter = (int)(temp - active_char_set->characters);

            else
                current_letter = 0;
        }
        else
            current_letter = 0;

        printf(" - %d\n", current_letter);

        if (input_text[current_offset] != active_char_set->characters[current_letter])
        {
            input_rem_char();
            input_text[current_offset] = active_char_set->characters[current_letter];
            input_add_char();
        }
    }
    else
        printf("\n");

    input_dump_state();
}


void input_next_letter(int amount)
{
    printf("input_next_letter(%d)\n", amount);
    if (active_char_set)
    {
        current_letter = (active_char_set->characters_len + (current_letter + amount)) % active_char_set->characters_len;

        if (current_letter < 0)
            current_letter += active_char_set->characters_len;

        input_rem_char();
        input_text[current_offset] = active_char_set->characters[current_letter];
        input_add_char();

        input_dump_state();
    }
}


void input_prev_letter(int amount)
{
    printf("input_prev_letter(%d)\n", amount);
    if (active_char_set)
    {
        current_letter = (active_char_set->characters_len + (current_letter - amount)) % active_char_set->characters_len;

        if (current_letter < 0)
            current_letter += active_char_set->characters_len;

        input_rem_char();
        input_text[current_offset] = active_char_set->characters[current_letter];
        input_add_char();

        input_dump_state();
    }
}


void input_next_word(int amount)
{
    if (active_word_set)
    {
        current_word = (active_word_set->words_len + (current_word + amount)) % active_word_set->words_len;

        input_rem_word();
        input_add_word(active_word_set->words[current_word], strlen(active_word_set->words[current_word]));

        input_dump_state();
    }
}


void input_prev_word(int amount)
{
    if (active_word_set)
    {
        current_word = (active_word_set->words_len + (current_word - amount)) % active_word_set->words_len;

        input_rem_word();
        input_add_word(active_word_set->words[current_word], strlen(active_word_set->words[current_word]));

        input_dump_state();
    }
}
