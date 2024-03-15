/* Copyright (c) 2021-2023
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

void UINPUT_SET_ABS_P(
    struct uinput_user_dev* dev,
    int axis,
    int min,
    int max,
    int fuzz,
    int flat)
{
    dev->absmax[axis] = max;
    dev->absmin[axis] = min;
    dev->absfuzz[axis] = fuzz;
    dev->absflat[axis] = flat;
}

void setupFakeXbox360Device(struct uinput_user_dev *device, int fd)
{
    strncpy(device->name, XBOX_CONTROLLER_NAME, UINPUT_MAX_NAME_SIZE);
    device->id.vendor = 0x045e;  /* sample vendor */
    device->id.product = 0x028e; /* sample product */

    if (
            ioctl(fd, UI_SET_EVBIT, EV_KEY) || ioctl(fd, UI_SET_EVBIT, EV_SYN) ||
            ioctl(fd, UI_SET_EVBIT, EV_ABS) ||
            // X-Box 360 pad buttons
            ioctl(fd, UI_SET_KEYBIT, BTN_A) || ioctl(fd, UI_SET_KEYBIT, BTN_B) ||
            ioctl(fd, UI_SET_KEYBIT, BTN_X) || ioctl(fd, UI_SET_KEYBIT, BTN_Y) ||
            ioctl(fd, UI_SET_KEYBIT, BTN_TL) || ioctl(fd, UI_SET_KEYBIT, BTN_TR) ||
            ioctl(fd, UI_SET_KEYBIT, BTN_THUMBL) ||
            ioctl(fd, UI_SET_KEYBIT, BTN_THUMBR) ||
            ioctl(fd, UI_SET_KEYBIT, BTN_SELECT) ||
            ioctl(fd, UI_SET_KEYBIT, BTN_START) || ioctl(fd, UI_SET_KEYBIT, BTN_MODE) ||
            // absolute (sticks)
            ioctl(fd, UI_SET_ABSBIT, ABS_X) ||
            ioctl(fd, UI_SET_ABSBIT, ABS_Y) ||
            ioctl(fd, UI_SET_ABSBIT, ABS_RX) ||
            ioctl(fd, UI_SET_ABSBIT, ABS_RY) ||
            ioctl(fd, UI_SET_ABSBIT, ABS_Z) ||
            ioctl(fd, UI_SET_ABSBIT, ABS_RZ) ||
            ioctl(fd, UI_SET_ABSBIT, ABS_HAT0X) ||
            ioctl(fd, UI_SET_ABSBIT, ABS_HAT0Y)) {
        printf("Failed to configure fake Xbox 360 controller\n");
        exit(-1);
    }

    UINPUT_SET_ABS_P(device, ABS_X, -32768, 32767, 16, 128);
    UINPUT_SET_ABS_P(device, ABS_Y, -32768, 32767, 16, 128);
    UINPUT_SET_ABS_P(device, ABS_RX, -32768, 32767, 16, 128);
    UINPUT_SET_ABS_P(device, ABS_RY, -32768, 32767, 16, 128);
    UINPUT_SET_ABS_P(device, ABS_HAT0X, -1, 1, 0, 0);
    UINPUT_SET_ABS_P(device, ABS_HAT0Y, -1, 1, 0, 0);
    UINPUT_SET_ABS_P(device, ABS_Z, 0, 255, 0, 0);
    UINPUT_SET_ABS_P(device, ABS_RZ, 0, 255, 0, 0);
}


void handleEventBtnFakeXbox360Device(const SDL_Event *event, bool is_pressed)
{
    // Fake Xbox360 mode
    switch (event->cbutton.button) {
    case SDL_CONTROLLER_BUTTON_A:
        emitKey(BTN_A, is_pressed, 0);
        break;

    case SDL_CONTROLLER_BUTTON_B:
        emitKey(BTN_B, is_pressed, 0);
        break;

    case SDL_CONTROLLER_BUTTON_X:
        emitKey(BTN_X, is_pressed, 0);
        break;

    case SDL_CONTROLLER_BUTTON_Y:
        emitKey(BTN_Y, is_pressed, 0);
        break;

    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        emitKey(BTN_TL, is_pressed, 0);
        break;

    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        emitKey(BTN_TR, is_pressed, 0);
        break;

    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        emitKey(BTN_THUMBL, is_pressed, 0);
        break;

    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        emitKey(BTN_THUMBR, is_pressed, 0);
        break;

    case SDL_CONTROLLER_BUTTON_BACK: // aka select
        emitKey(BTN_SELECT, is_pressed, 0);
        break;

    case SDL_CONTROLLER_BUTTON_GUIDE:
        emitKey(BTN_MODE, is_pressed, 0);
        break;

    case SDL_CONTROLLER_BUTTON_START:
        emitKey(BTN_START, is_pressed, 0);
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        emitAxisMotion(ABS_HAT0Y, is_pressed ? -1 : 0);
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        emitAxisMotion(ABS_HAT0Y, is_pressed ? 1 : 0);
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        emitAxisMotion(ABS_HAT0X, is_pressed ? -1 : 0);
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        emitAxisMotion(ABS_HAT0X, is_pressed ? 1 : 0);
        break;
    }
}

void handleEventAxisFakeXbox360Device(const SDL_Event *event)
{
    switch (event->caxis.axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        emitAxisMotion(ABS_X, event->caxis.value);
        break; 

    case SDL_CONTROLLER_AXIS_LEFTY:
        emitAxisMotion(ABS_Y, event->caxis.value);
        break;

    case SDL_CONTROLLER_AXIS_RIGHTX:
        emitAxisMotion(ABS_RX, event->caxis.value);
        break;

    case SDL_CONTROLLER_AXIS_RIGHTY:
        emitAxisMotion(ABS_RY, event->caxis.value);
        break;

    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        // The target range for the triggers is 0..255 instead of
        // 0..32767, so we shift down by 7 as that does exactly the
        // scaling we need (32767 >> 7 is 255)
        emitAxisMotion(ABS_Z, event->caxis.value >> 7);
        break;

    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        emitAxisMotion(ABS_RZ, event->caxis.value >> 7);
        break;
    }
}
