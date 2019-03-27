#pragma once

#include "BlackRoot\Pubc\JSON.h"

namespace Toolbox {
namespace Core {

	class ILogMan {
	public:
        virtual ~ILogMan() { ; }

        virtual void Initialise(BlackRoot::Format::JSON &) = 0;
        virtual void Deinitialise(BlackRoot::Format::JSON &) = 0;
	};

}
}