#pragma once

#include "Toolbox/Interface LogMan.h"

namespace Toolbox {
namespace Base {

	class LogMan : public Toolbox::Core::ILogMan {
	public:
        ~LogMan() override { ; }

        void Initialise(BlackRoot::Format::JSON & param) override;
        void Deinitialise(BlackRoot::Format::JSON & param) override;
	};

}
}