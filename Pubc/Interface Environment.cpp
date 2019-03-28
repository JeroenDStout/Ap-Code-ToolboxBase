#include "ToolboxBase/Pubc/Interface Environment.h"

namespace Toolbox {
namespace Core {

	static IEnvironment * GlobalEnvironment = nullptr;

	IEnvironment * GetEnvironment()
	{
		return GlobalEnvironment;
	}

	void SetEnvironment(IEnvironment *manager)
	{
		GlobalEnvironment = manager;
	}

}
}

using namespace Toolbox::Core;

IEnvironment::IEnvironment()
{
    this->LogMan        = nullptr;
    this->SocketMan     = nullptr;
}