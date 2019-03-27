#pragma once

#include "BlackRoot\Pubc\JSON.h"
#include "Toolbox\Pubc\Interface Messages.h"

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