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


void handleInputEvent(const SDL_Event *event)
{
    // Main input loop
    switch (event->type)
    {
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        {
            const bool pressed = event->type == SDL_CONTROLLERBUTTONDOWN;

            if (xbox360_mode)
            {
                handleEventBtnFakeXbox360Device(event, pressed);
            }

            handleEventBtnFakeKeyboardMouseDevice(event, pressed);
        }
        break;

    case SDL_CONTROLLERAXISMOTION:
        if (xbox360_mode)
        {
            handleEventAxisFakeXbox360Device(event);
        }
        else
        {
            handleEventAxisFakeKeyboardMouseDevice(event);
        }
        break;

    case SDL_CONTROLLERDEVICEADDED:
        {
            SDL_GameController* controller = SDL_GameControllerOpen(event->cdevice.which);
            if (controller)
            {
                int controller_fd = interpose_get_fd();
                SDL_Joystick *joystick = SDL_GameControllerGetJoystick(controller);
                const char *name = SDL_JoystickName(joystick);
                printf("Joystick %i has game controller name '%s': %d\n", 0, name, controller_fd);
                if (strcmp(name, XBOX_CONTROLLER_NAME) != 0)
                {
                    SDL_GameControllerOpen(event->cdevice.which);
                    controller_add_fd(event->cdevice.which, controller_fd);
                }
                else
                {
                    SDL_GameControllerClose(controller);
                }
            }
        }
        break;

    case SDL_CONTROLLERDEVICEREMOVED:
        {
            SDL_GameController* controller = SDL_GameControllerFromInstanceID(event->cdevice.which);
            if (controller)
            {
                controller_remove_fd(event->cdevice.which);
                SDL_GameControllerClose(controller);
            }
        }
        break;

    case SDL_QUIT:
        current_state.running = false;
        return;
    }
}
