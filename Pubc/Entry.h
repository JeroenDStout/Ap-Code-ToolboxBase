/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "ToolboxBase/Pubc/Environment Bootstrap.h"

// -------- Windows

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <string.h>

#pragma warning(disable: 4996)

#define TB_STARTFUNC(x, y) \
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { \
    return x(&y, __argc, __argv); \
}
#endif

// -------- Implement

namespace Toolbox {
namespace Core {

    using StartupFunction = int(*)(Toolbox::Util::EnvironmentBootstrap&);

    int DefaultStart(StartupFunction, int argc, char* argv[]);
    
#define TB_STARTFUNC_DEF(x) \
    TB_STARTFUNC(Toolbox::Core::DefaultStart, x);

}
}