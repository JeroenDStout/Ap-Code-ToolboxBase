/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "BlackRoot/Pubc/JSON.h"
#include "BlackRoot/Pubc/Files.h"

#include "Conduits/Pubc/Interface Raw Message.h"

namespace Toolbox {
namespace Core {

	class IEnvironment;
	class ISocketman;
	class ILogman;

	IEnvironment * Get_Environment();
	void Set_Environment(IEnvironment *);

	class IEnvironment {
	public:
        using FilePath = BlackRoot::IO::FilePath;

		ILogman		*Logman;
		ISocketman	*Socketman;

        IEnvironment();
        virtual ~IEnvironment() { ; }

            // Setup

        virtual void set_boot_dir(FilePath) = 0;
        virtual void set_ref_dir(FilePath) = 0;
        
            // Control

        virtual void run_with_current_thread() = 0;
        virtual void async_close() = 0;

        virtual void async_receive_message(Conduits::Raw::IRelayMessage *) = 0;

            // Util

        virtual bool get_is_running() = 0;

            // Info

        virtual FilePath get_ref_dir() = 0;

        /// ???

        //virtual void ReceiveDelegateMessageFromSocket(std::weak_ptr<void>, std::string) = 0;
        //virtual void ReceiveDelegateMessageToSocket(std::weak_ptr<void>, Messaging::IAsynchMessage *) = 0;

        //virtual void ReceiveDelegateMessageAsync(Messaging::IAsynchMessage*) = 0;
	};

}
}