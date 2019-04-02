/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "ToolboxBase/Pubc/Interface LogMan.h"

namespace Toolbox {
namespace Base {

	class LogMan : public Toolbox::Core::ILogMan {
	public:
        ~LogMan() override { ; }

        void Initialise(BlackRoot::Format::JSON & param) override;
        void Deinitialise(BlackRoot::Format::JSON & param) override;
	};

}
}