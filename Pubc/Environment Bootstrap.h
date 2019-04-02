/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "ToolboxBase/Pubc/Interface Environment.h"

namespace Toolbox {
namespace Util {

    struct EnvironmentBootstrap {
        Toolbox::Core::IEnvironment * Environment;

        EnvironmentBootstrap(Toolbox::Core::IEnvironment *env)
        : Environment(env) {
        }

        bool HasStartupFile() {
            return false;
        }

        bool ExecuteFromString(std::string);
    };

}
}