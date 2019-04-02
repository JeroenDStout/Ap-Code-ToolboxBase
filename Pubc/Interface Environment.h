/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "BlackRoot/Pubc/JSON.h"
#include "ToolboxBase/Pubc\Interface Messages.h"

namespace Toolbox {
namespace Core {

	class IEnvironment;
	class ISocketMan;
	class ILogMan;

	IEnvironment * GetEnvironment();
	void SetEnvironment(IEnvironment *);

	class IEnvironment : public virtual Messaging::IMessageReceiver {
	public:
		ILogMan		*LogMan;
		ISocketMan	*SocketMan;

        IEnvironment();
        virtual ~IEnvironment() { ; }

        virtual void Run() = 0;
        virtual void Close() = 0;

        virtual void ReceiveDelegateMessageFromSocket(std::weak_ptr<void>, std::string) = 0;
        virtual void ReceiveDelegateMessageToSocket(std::weak_ptr<void>, Messaging::IAsynchMessage *) = 0;

        virtual void ReceiveDelegateMessageAsync(Messaging::IAsynchMessage*) = 0;

        virtual bool IsRunning() = 0;

        virtual void UnloadAll() = 0;

        virtual ILogMan *    AllocateLogMan() = 0;
        virtual ISocketMan * AllocateSocketMan() = 0;
	};

}
}