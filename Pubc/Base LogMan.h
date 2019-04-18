/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "Conduits/Pubc/Savvy Relay Receiver.h"

#include "ToolboxBase/Pubc/Interface Logman.h"

namespace Toolbox {
namespace Base {

	class Logman : public Toolbox::Core::ILogman, public Conduits::SavvyRelayMessageReceiver {
        CON_RMR_DECLARE_CLASS(Logman, ILogman);

        using JSON = BlackRoot::Format::JSON;

	public:
        ~Logman() override { ; }

        void initialise(const JSON param) override;
        void deinitialise(const JSON param) override;
	};

}
}