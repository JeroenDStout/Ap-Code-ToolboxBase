/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ToolboxBase/Pubc/Interface Environment.h"

namespace Toolbox {
namespace Core {

	static IEnvironment * Global_Environment = nullptr;

	IEnvironment * Get_Environment()
	{
		return Global_Environment;
	}

	void Set_Environment(IEnvironment *manager)
	{
		Global_Environment = manager;
	}

}
}

using namespace Toolbox::Core;

IEnvironment::IEnvironment()
{
    this->Logman    = nullptr;
    this->Socketman = nullptr;
}