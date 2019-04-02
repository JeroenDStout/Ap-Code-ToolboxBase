/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "BlackRoot\Pubc\JSON.h"
#include "ToolboxBase/Pubc\Interface Messages.h"

namespace Toolbox {
namespace Core {

	class ISocketMan {
	public:
        virtual ~ISocketMan() { ; }

        virtual void Initialise(BlackRoot::Format::JSON * param) = 0;
        virtual void Deinitialise(BlackRoot::Format::JSON * param) = 0;

        virtual void MessageSendImmediate(std::weak_ptr<void>, std::string) = 0;
	};

}
}