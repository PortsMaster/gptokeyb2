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


#define DEFAULT_CONFIG "~/.config/gptokeyb2.ini"

int uinp_fd=0;
bool xbox360_mode=false;
bool config_mode=false;
gptokeyb_config *default_config=NULL;

struct uinput_user_dev uidev;


int main(int argc, char* argv[])
{
    config_init();

    bool done = false;
    char opt;
    char default_control[MAX_CONTROL_NAME] = "";

    while ((!done) && ((opt = getopt(argc, argv, "xp:c:")) != -1))
    {
        switch (opt)
        {
        case 'c':
            config_load(optarg, false);
            config_mode = true;
            xbox360_mode = false;
            break;

        case 'x':
            config_mode = false;
            xbox360_mode = true;
            break;

        case 'p':
            strncpy(default_control, optarg, MAX_CONTROL_NAME-1);
            printf("using control %s\n", default_control);
            break;

        case '?':
            if (optopt == 'c')
                fprintf (stderr, "Option -%c requires an argument.\n", optopt);
            else if (isprint (optopt))
                fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
            return 1;

        default:
            printf("%d\n", opt);
            done = true;
            break;
            // fprintf(stderr, "Usage: %s [-c <config.ini>] [-p control_mode]\n", argv[0]);
            // config_quit();
            // exit(EXIT_FAILURE);
        }
    }

    if (config_mode)
    {
        if (access(DEFAULT_CONFIG, F_OK) == 0)
        {
            printf("Loading '%s'\n", DEFAULT_CONFIG);

            if (config_load(DEFAULT_CONFIG, true))
            {
                config_quit();
                return 1;
            }
        }

        if (strcmp(default_control, "") != 0)
        {
            default_config = config_find(default_control);
        }

        if (default_config == NULL)
        {
            default_config = root_config;
        }
    }

    config_dump();

    // SDL initialization and main loop
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0)
    {
        printf("SDL_Init() failed: %s\n", SDL_GetError());
        return -1;
    }

    // Create fake input device (not needed in kill mode)
    //if (!kill_mode) {  
    if (config_mode || xbox360_mode)
    {   // initialise device, even in kill mode, now that kill mode will work with config & xbox modes
        uinp_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

        if (uinp_fd < 0)
        {
            printf("Unable to open /dev/uinput\n");
            return -1;
        }

        // Intialize the uInput device to NULL
        memset(&uidev, 0, sizeof(uidev));
        uidev.id.version = 1;
        uidev.id.bustype = BUS_USB;

        if (xbox360_mode)
        {
            printf("Running in Fake Xbox 360 Mode\n");
            setupFakeXbox360Device(&uidev, uinp_fd);
        }
        else
        {
            printf("Running in Fake Keyboard mode\n");
            setupFakeKeyboardMouseDevice(&uidev, uinp_fd);
        }

        // Create input device into input sub-system
        write(uinp_fd, &uidev, sizeof(uidev));

        if (ioctl(uinp_fd, UI_DEV_CREATE)) {
            printf("Unable to create UINPUT device.");
            return -1;
        }
    }

    const char* db_file = SDL_getenv("SDL_GAMECONTROLLERCONFIG_FILE");
    if (db_file)
    {
        SDL_GameControllerAddMappingsFromFile(db_file);
    }

    SDL_Event event;
    bool running = false;
    int mouse_x = 0;
    int mouse_y = 0;

    while (running)
    {
        if (!SDL_WaitEvent(&event))
        {
            printf("SDL_WaitEvent() failed: %s\n", SDL_GetError());
            return -1;
        }

        running = handleInputEvent(&event);
    }

    SDL_Quit();

    /*
        * Give userspace some time to read the events before we destroy the
        * device with UI_DEV_DESTROY.
        */
    sleep(1);

    /* Clean up */
    ioctl(uinp_fd, UI_DEV_DESTROY);
    close(uinp_fd);

    return 0;
}
