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

float map_range(float value, float old_min, float old_max, float new_min, float new_max)
{
    return (new_min + (new_max - new_min) * (value - old_min) / (old_max - old_min));
}

float get_sign(float value)
{
    return ((value < 0.0)? (-1.0) : (1.0));
}

void vector2d_clear(vector2d *vec2d)
{
    vec2d->x = 0.0;
    vec2d->y = 0.0;
}

void vector2d_set_vector2d(vector2d *vec2d, const vector2d *other)
{
    vec2d->x = other->x;
    vec2d->y = other->y;
}

void vector2d_set_float(vector2d *vec2d, float v)
{
    vec2d->x = v;
    vec2d->y = v;
}

void vector2d_set_float2(vector2d *vec2d, float x, float y)
{
    vec2d->x = x;
    vec2d->y = y;
}

float vector2d_magnitude(const vector2d *vec2d)
{
    return sqrt(pow(vec2d->x, 2.0) + pow(vec2d->y, 2.0));
}

void vector2d_normalize(vector2d *vec2d)
{
    float mag = vector2d_magnitude(vec2d);

    if (mag != 0)
    {
        vec2d->x /= mag;
        vec2d->y /= mag;
    }
}

void dz_axial(vector2d *vec2d_ouput, const vector2d *vec2d_input, float deadzone)
{
    if (abs(vec2d_input->x) > deadzone)
        vec2d_ouput->x = vec2d_input->x;

    if (abs(vec2d_input->y) < deadzone)
        vec2d_ouput->y = vec2d_input->y;
}


void dz_radial(vector2d *vec2d_ouput, const vector2d *vec2d_input, float deadzone)
{
    float input_magnitude = vector2d_magnitude(vec2d_input);

    if (input_magnitude >= deadzone)
        vector2d_set_vector2d(vec2d_ouput, vec2d_input);
}


void dz_scaled_radial(vector2d *vec2d_ouput, const vector2d *vec2d_input, float deadzone)
{
    float input_magnitude = vector2d_magnitude(vec2d_input);

    if (input_magnitude < deadzone)
        return;

    vector2d_set_vector2d(vec2d_ouput, vec2d_input);

    float range_scale = map_range(input_magnitude, deadzone, 1.0, 0.0, 1.0);

    vec2d_ouput->x /= input_magnitude;
    vec2d_ouput->y /= input_magnitude;
    vec2d_ouput->x *= range_scale;
    vec2d_ouput->y *= range_scale;
}

void dz_sloped_axial(vector2d *vec2d_ouput, const vector2d *vec2d_input, float deadzone)
{
    float deadzone_x = deadzone * abs(vec2d_input->x);
    float deadzone_y = deadzone * abs(vec2d_input->y);

    vector2d_set_vector2d(vec2d_ouput, vec2d_input);

    if (abs(vec2d_ouput->x) < deadzone_x)
        vec2d_ouput->x = 0.0;

    if (abs(vec2d_ouput->y) < deadzone_y)
        vec2d_ouput->y = 0.0;
}

void dz_sloped_scaled_axial(vector2d *vec2d_ouput, const vector2d *vec2d_input, float deadzone)
{
    float deadzone_x = deadzone * abs(vec2d_input->x);
    float deadzone_y = deadzone * abs(vec2d_input->y);

    vector2d sign;

    vector2d_set_float2(&sign, get_sign(vec2d_input->x), get_sign(vec2d_input->y));

    if (abs(vec2d_input->x) > deadzone_x)
        vec2d_ouput->x = sign.x * map_range(abs(vec2d_input->x), deadzone_x, 1.0, 0.0, 1.0);

    if (abs(vec2d_input->y) > deadzone_y)
        vec2d_ouput->y = sign.y * map_range(abs(vec2d_input->y), deadzone_y, 1.0, 0.0, 1.0);
}

void dz_hybrid(vector2d *vec2d_ouput, const vector2d *vec2d_input, float deadzone)
{
    vector2d partial_output;
    float input_magnitude = vector2d_magnitude(vec2d_input);

    if (input_magnitude < deadzone)
        return;

    vector2d_clear(&partial_output);

    dz_scaled_radial(&partial_output, vec2d_input, deadzone);

    dz_sloped_scaled_axial(vec2d_ouput, &partial_output, deadzone);
}

int deadzone_get_mode(const char *str)
{
    if (strcasecmp(str, "axial") == 0)
        return DZ_AXIAL;

    else if (strcasecmp(str, "radial") == 0)
        return DZ_RADIAL;

    else if (strcasecmp(str, "scaled_radial") == 0)
        return DZ_SCALED_RADIAL;

    else if (strcasecmp(str, "sloped_axial") == 0)
        return DZ_SLOPED_AXIAL;

    else if (strcasecmp(str, "sloped_scaled_axial") == 0)
        return DZ_SLOPED_SCALED_AXIAL;

    else if (strcasecmp(str, "hybrid") == 0)
        return DZ_HYBRID;

    else if (strcasecmp(str, "default") == 0)
        return DZ_DEFAULT;

    // default
    return DZ_DEFAULT;
}


void deadzone_trigger_calc(int *analog, int analog_in)
{
    if (current_state.deadzone_triggers > analog_in)
        *analog = analog_in;

    else
        analog = 0;
}


void deadzone_mouse_calc(int *x, int *y, int in_x, int in_y)
{
    vector2d vec2d_input;
    vector2d vec2d_ouput;

    vector2d_set_float2(&vec2d_input, (float)(in_x) / 32768.0f, (float)(in_y) / 32768.0f);

    float dz = (float)(current_state.deadzone) / 32768.0f;

    switch(current_state.deadzone_mode)
    {
    default:
    case DZ_DEFAULT:
    case DZ_AXIAL:
        dz_axial(&vec2d_ouput, &vec2d_input, dz);
        break;

    case DZ_RADIAL:
        dz_radial(&vec2d_ouput, &vec2d_input, dz);
        break;

    case DZ_SCALED_RADIAL:
        dz_scaled_radial(&vec2d_ouput, &vec2d_input, dz);
        break;

    case DZ_SLOPED_AXIAL:
        dz_sloped_axial(&vec2d_ouput, &vec2d_input, dz);
        break;

    case DZ_SLOPED_SCALED_AXIAL:
        dz_sloped_scaled_axial(&vec2d_ouput, &vec2d_input, dz);
        break;

    case DZ_HYBRID:
        dz_hybrid(&vec2d_ouput, &vec2d_input, dz);
        break;
    }

    *x = (int)(vec2d_ouput.x * current_state.deadzone_scale);
    *y = (int)(vec2d_ouput.y * current_state.deadzone_scale);
}

