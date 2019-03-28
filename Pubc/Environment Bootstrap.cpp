#include "BlackRoot\Pubc\Threaded IO Stream.h"

#include "ToolboxBase/Pubc\Base Messages.h"
#include "ToolboxBase/Pubc\Environment Bootstrap.h"

using namespace Toolbox::Util;

bool EnvironmentBootstrap::ExecuteFromString(const std::string str)
{ 
    using cout = BlackRoot::Util::Cout;

    auto json = Toolbox::Messaging::JSON::parse(str);

    for (auto& el1 : json.items()) {
        if (0 == (el1.key().compare("serious"))) {
            for (auto& el2 : el1.value().items()) {
                cout{} <<  "> " << el2.value().dump() << std::endl;

                Toolbox::Messaging::AwaitMessage msg(el2.value());
                this->Environment->ReceiveDelegateMessageAsync(&msg);

                if (!msg.AwaitSuccess()) {
                    cout{} << "<## " << msg.Response.dump() << std::endl
                           << "    Error in serious startup!" << std::endl;
                    return false;
                }
            
                cout{} << "< " << msg.Response.dump() << std::endl;
            }
        }
        else {
            cout{} << "Bootstrap does not understand '" << el1.key() << "!" << std::endl;
            return false;
        }
    }

    return true;
}