#include "ToolboxBase\Pubc\Message Relay.h"

using namespace Toolbox::Messaging;

void MessageRelayAsDir::RelayMessageSafe(Toolbox::Messaging::IAsynchMessage *message)
{
    std::string target = "";

    if (message->Message.is_object()) {
        target = message->Message.begin().key();
    }
    else {
        message->SetFailed();
        message->Response = { "Invalid command type" };
        message->Close();
        return;
    }

    if (target.find("/") != std::string::npos) {
        message->SetFailed();
        message->Response = { "~~~ no code ~~~" };
        message->Close();
        return;
    }

    auto it = this->RelayMap.find(target);
    if (it == this->RelayMap.end()) {
        std::stringstream reply;
        reply << "The target '" << target << "' does not exist.";

        message->SetFailed();
        message->Response = { "No Entity", reply.str() };
        message->Close();
        return;
    }

    message->Message = message->Message.begin().value();
    it->second.DeliverMethod(target, message);
}