/*
 *
 *  © Stout Games 2019
 *
 */

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