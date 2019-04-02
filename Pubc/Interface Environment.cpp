/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ToolboxBase/Pubc/Interface Environment.h"

namespace Toolbox {
namespace Core {

	static IEnvironment * GlobalEnvironment = nullptr;

	IEnvironment * GetEnvironment()
	{
		return GlobalEnvironment;
	}

	void SetEnvironment(IEnvironment *manager)
	{
		GlobalEnvironment = manager;
	}

}
}

using namespace Toolbox::Core;

IEnvironment::IEnvironment()
{
    this->LogMan        = nullptr;
    this->SocketMan     = nullptr;
}