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


void setupFakeKeyboardMouseDevice(struct uinput_user_dev *device, int fd)
{
    strncpy(device->name, "Fake Keyboard", UINPUT_MAX_NAME_SIZE);
    device->id.vendor = 0x1234;  /* sample vendor */
    device->id.product = 0x5678; /* sample product */

    for (int i = 0; i < 256; i++)
    {
        ioctl(fd, UI_SET_KEYBIT, i);
    }

    // Keys or Buttons
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);

    // Fake mouse
    ioctl(fd, UI_SET_EVBIT, EV_REL);
    ioctl(fd, UI_SET_RELBIT, REL_X);
    ioctl(fd, UI_SET_RELBIT, REL_Y);
    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
}


void handleEventBtnFakeKeyboardMouseDevice(const SDL_Event *event, bool is_pressed)
{
    //config mode (i.e. not textinputinteractive_mode_active)
    switch (event->cbutton.button)
    {
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        break;

    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        break;

    case SDL_CONTROLLER_BUTTON_A:
        break;

    case SDL_CONTROLLER_BUTTON_B:
        break;

    case SDL_CONTROLLER_BUTTON_X:
        break;

    case SDL_CONTROLLER_BUTTON_Y:
        break;

    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        break;

    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        break;

    case SDL_CONTROLLER_BUTTON_LEFTSTICK:
        break;

    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
        break;

    case SDL_CONTROLLER_BUTTON_GUIDE:
        break;

    case SDL_CONTROLLER_BUTTON_BACK: // aka select
        break;

    case SDL_CONTROLLER_BUTTON_START:
        break;
    } //switch
}


void handleEventAxisFakeKeyboardMouseDevice(const SDL_Event *event)
{
    switch (event->caxis.axis) {
    case SDL_CONTROLLER_AXIS_LEFTX:
        break;

    case SDL_CONTROLLER_AXIS_LEFTY:
        break;

    case SDL_CONTROLLER_AXIS_RIGHTX:
        break;

    case SDL_CONTROLLER_AXIS_RIGHTY:
        break;

    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
        break;

    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
        break;
    } // switch (event.caxis.axis)
}
