/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "BlackRoot/Pubc/JSON.h"

#include "Conduits/Pubc/Interface Relay Receiver.h"

namespace Toolbox {
namespace Core {

	class ILogman : public virtual Conduits::IRelayMessageReceiver {
	public:
        using JSON = BlackRoot::Format::JSON;

        virtual ~ILogman() { ; }

        virtual void initialise(const JSON) = 0;
        virtual void deinitialise(const JSON) = 0;
	};

}
}