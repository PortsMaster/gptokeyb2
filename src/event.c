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

static SDL_JoystickID existing_controllers[MAX_CONTROLLERS];
static int num_existing_controllers = 0;
static SDL_JoystickID virtual_controller_id = -1;

// Helper function to check if an ID exists in the array
bool isExistingController(SDL_JoystickID id) {
    for (int i = 0; i < num_existing_controllers; i++) {
        if (existing_controllers[i] == id) {
            return true;
        }
    }
    return false;
}

// Record controllers before initializing virtual controller so that later the virtual controller can be detected when checked against this list
void recordExistingControllers() {
    num_existing_controllers = 0;

    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks && num_existing_controllers < MAX_CONTROLLERS; i++) {
        SDL_GameController* controller = SDL_GameControllerOpen(i);
        if (controller) {
            SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
            SDL_JoystickID id = SDL_JoystickInstanceID(joystick);
            existing_controllers[num_existing_controllers++] = id;
            SDL_GameControllerClose(controller);
        }
    }

    printf("Recorded %d existing controllers\n", num_existing_controllers);
}

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
        {
            if (xbox360_mode)
            {
                handleEventAxisFakeXbox360Device(event);
            }

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
                SDL_JoystickID instance_id = SDL_JoystickInstanceID(joystick);
                const char *name = SDL_JoystickName(joystick);
                printf("Joystick %i has game controller name '%s': %d", 0, name, controller_fd);
                if (xbox360_mode && ((virtual_controller_id == -1 && !isExistingController(instance_id)) || instance_id == virtual_controller_id) ) {
                    virtual_controller_id = instance_id;
                    printf(" closed because it's our own controller.\n");
                    SDL_GameControllerClose(controller);
                }
                else
                {
                    printf(" opened.\n");
                    controller_add_fd(event->cdevice.which, controller_fd);
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
