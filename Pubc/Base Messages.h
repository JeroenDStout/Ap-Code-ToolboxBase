/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "BlackRoot\Pubc\JSON.h"
#include "ToolboxBase/Pubc\Interface Messages.h"

#include <mutex>
#include <condition_variable>
#include <iostream>

namespace Toolbox {
namespace Messaging {

    class BaseMessageReceiver : public virtual IMessageReceiver {
        TB_MESSAGES_DECLARE_RECEIVER(BaseMessageReceiver, IMessageReceiver)

        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(dir);
        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(http);
    };

    class SocketMessage : public IAsynchMessage {
    protected:
        std::weak_ptr<void>     SocketRef;

    public:
        SocketMessage(std::weak_ptr<void>, BlackRoot::Format::JSON);
        ~SocketMessage() { ; }

        void SetOK() override;
        void SetFailed() override;
        void Close() override;
    };

    class AwaitMessage : public IAsynchMessage {
    protected:
        std::mutex                  WaitMutex;
        std::condition_variable     ClosedCV;
        bool                        Closed;

    public:
        AwaitMessage();
        AwaitMessage(BlackRoot::Format::JSON);
        ~AwaitMessage() { ; }

        void SetOK() override;
        void SetFailed() override;
        void Close() override;
        
        bool IsClosed() { return this->Closed; }

        IAsynchMessage::StateType::Type Await();
        bool                            AwaitSuccess();
    };

}
}