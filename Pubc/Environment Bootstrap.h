/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "BlackRoot/Pubc/Files.h"
#include "BlackRoot/Pubc/JSON.h"
#include "ToolboxBase/Pubc/Interface Environment.h"

namespace Toolbox {
namespace Util {

    struct EnvironmentBootstrap {
        std::string     BootPath;

        Toolbox::Core::IEnvironment * Environment;

        EnvironmentBootstrap()
        : Environment(nullptr) {
        }

        bool ExecuteFromFilePath(const BlackRoot::IO::FilePath);
        bool ExecuteFromFile(BlackRoot::IO::IFileStream*);

        bool ExecuteFromBootFile() {
            if (BootPath.length() == 0)
                return false;
            return this->ExecuteFromFilePath(BootPath);
        }

        bool ExecuteFromJSON(BlackRoot::Format::JSON);
    };

}
}