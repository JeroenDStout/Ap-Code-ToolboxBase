#pragma once

/* © Stout Games 2018
 */

#include "BlackRoot/JSON.h"
#include "Toolbox\Pubc\Interface Messages.h"

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