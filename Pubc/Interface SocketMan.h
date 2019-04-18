/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "Conduits/Pubc/Interface Conduit.h"

#include "BlackRoot/Pubc/JSON.h"

namespace Toolbox {
namespace Core {
    
	class ISocketman : public virtual Conduits::IRelayMessageReceiver {
    public:
        using JSON  = BlackRoot::Format::JSON;
        using Nexus =  Conduits::Raw::INexus;

        virtual ~ISocketman() { ; }

        virtual void initialise(const JSON param) = 0;
        virtual void deinitialise_and_wait(const JSON param) = 0;
        
        virtual Nexus * get_en_passant_nexus() = 0;
        virtual void    connect_en_passant_conduit(Conduits::Raw::ConduitRef) = 0;
	};

}
}