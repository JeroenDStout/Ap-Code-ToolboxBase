/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <string.h>

#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Files Types.h"
#include "BlackRoot/Pubc/Sys Path.h"
#include "BlackRoot/Pubc/Version Reg.h"

#include "ToolboxBase/Pubc/Entry.h"

namespace fs = std::experimental::filesystem;

int Toolbox::Core::DefaultStart(StartupFunction f, int argc, char* argv[])
{
    Toolbox::Util::EnvironmentBootstrap bootstrap;

    bootstrap.Boot_Path = fs::current_path();

    std::string bootPathAppend = "";

    bool showConsole = true;
    bool ping        = false;
    
    for (int i = 1; i < argc; i++) {
        auto arg = argv[i];

        if (i == 1 && arg[0] != '-') {
                // if the first argument does not start with a '-'
                //  we assume it is the boot file; allowing boot files
                //  to be dragged on the exe
            
            bootPathAppend  = (arg);
        }

        if (0 == strncmp(arg, "-ping", 5)) {
                // replies with pong

            ping = true;
        }

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

            bootPathAppend  = (arg+3);
        }
    }

    if (bootPathAppend.length() > 0) {
        BlackRoot::IO::FilePath append = bootPathAppend;
        if (append.is_relative()) {
            bootstrap.Boot_Path /= bootPathAppend;
        }
        else {
            bootstrap.Boot_Path = bootPathAppend;
        }
        bootstrap.Boot_Path = fs::canonical(bootstrap.Boot_Path);
    }
    else {
        bootstrap.Boot_Path = "";
    }
    
#ifdef _WIN32
    if (showConsole) {
        if (AllocConsole()) {
            freopen("CONIN$", "r", stdin);
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
        }
        
        std::string title;

        const auto & version = BlackRoot::Repo::VersionRegistry::GetMainProjectVersion();
        title = version.Name;
        title += " (";
        title += version.Version;
        title += ")";

            // TODO: properly convert a utf-8 string to a wstring
        ::SetConsoleTitleA(title.c_str());
    }
#endif

    if (ping) {
        using cout = BlackRoot::Util::Cout;
        cout{} << "Pong" << std::endl << std::endl;
    }

    return f(bootstrap);
}