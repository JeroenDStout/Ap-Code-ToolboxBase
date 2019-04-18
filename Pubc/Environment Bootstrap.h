/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "BlackRoot/Pubc/Files Types.h"
#include "BlackRoot/Pubc/JSON.h"

#include "ToolboxBase/Pubc/Interface Environment.h"

namespace Toolbox {
namespace Util {

    struct EnvironmentBootstrap {
        using JSON   = BlackRoot::Format::JSON;
        using Path   = BlackRoot::IO::FilePath;
        using Stream = BlackRoot::IO::IFileStream;
        using Env    = Toolbox::Core::IEnvironment;

        Path  Boot_Path;
        Env   *Environment;

        EnvironmentBootstrap()
        : Environment(nullptr) {
        }

        void setup_environment(Env*);

        bool execute_from_file_path(const Path);
        bool execute_from_file(Stream*);

        bool execute_from_boot_file() {
            if (Boot_Path.empty())
                return false;
            return this->execute_from_file_path(Boot_Path);
        }

        bool execute_from_json(const JSON);
    };

}
}