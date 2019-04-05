/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ToolboxBase/Pubc/Entry.h"
#include <string.h>

int Toolbox::Core::DefaultStart(StartupFunction f, int argc, char* argv[])
{
    Toolbox::Util::EnvironmentBootstrap bootstrap;

    bootstrap.BootPath = "";

    bool showConsole = true;
    
    for (int i = 1; i < argc; i++) {
        auto arg = argv[i];

        if (0 == strncmp(arg, "-c", 2)) {
                // enables the console
                // -c     : console on
                // -c:on  : console on
                // -c:off : console off

            if (arg[2] == 0) {
                showConsole = true;
                continue;
            }
            if (0 == strncmp(arg+2, ":on", 3)) {
                showConsole = true;
            }
            else if (0 == strncmp(arg+2, ":off", 4)) {
                showConsole = false;
            }
        }

        if (0 == strncmp(arg, "-b:", 3)) {
                // selects the bootstrap file
                // -b:<path>

            bootstrap.BootPath  = (arg+3);
        }
    }
    
#ifdef _WIN32
    if (showConsole) {
        if (AllocConsole()) {
            freopen("CONIN$", "r", stdin);
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
        }
    }
#endif

    return f(bootstrap);
}