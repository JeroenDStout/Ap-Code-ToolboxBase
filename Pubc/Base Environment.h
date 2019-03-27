#pragma once

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <vector>

#include "BlackRoot/JSON.h"
#include "Toolbox/Interface Environment.h"
#include "Toolbox/Base Messages.h"
#include "Toolbox/Message Relay.h"

namespace Toolbox {
namespace Base {

    class SocketMan;
    class LogMan;

    class BaseEnvironment : public Core::IEnvironment, public Messaging::BaseMessageReceiver {
    public:
        TB_MESSAGES_DECLARE_RECEIVER(BaseEnvironment, Toolbox::Messaging::BaseMessageReceiver);

    protected:
        struct StateType {
            typedef unsigned char Type;
            enum : Type {
                Running     = 0,
                ShouldClose = 1,
                Closed      = 2
            };
        };

        struct __BaseStats {
            std::chrono::high_resolution_clock::time_point  StartTime;
        } BaseStats;
        
        Toolbox::Messaging::MessageRelayAsDir   MessageRelay;

        std::atomic<StateType::Type>  CurrentState;

        std::mutex                    MessageCueMutex;
        std::vector<Messaging::IAsynchMessage*> MessageCue;

        std::mutex                    MessageWaitMutex;
        std::condition_variable       MessageCV;

    public:
        virtual void InternalMessageLoop();
        virtual void InternalHandleMessage(Messaging::IAsynchMessage*);

        virtual void InternalSetupRelayMap();
        
        virtual void InternalInitStats();
        virtual void InternalCompileStats(BlackRoot::Format::JSON &);

        bool InternalThreadsShouldBeAwake();
        
        void InternalMessageRelayToNone(std::string, Messaging::IAsynchMessage*);
        void InternalMessageSendToNone(std::string, Messaging::IAsynchMessage*);

        void InternalMessageSendToEnv(std::string, Messaging::IAsynchMessage*);

    public:
        BaseEnvironment();
        ~BaseEnvironment();
        
        void ReceiveDelegateMessageAsync(Messaging::IAsynchMessage*) override;
        void ReceiveDelegateMessageFromSocket(std::weak_ptr<void>, std::string) override;
        void ReceiveDelegateMessageToSocket(std::weak_ptr<void>, Messaging::IAsynchMessage *) override;
        
        void Run() override;
        void Close() override;

        bool IsRunning() override;

        void UnloadAll() override;
        
        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(ping);
        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(stats);
        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(createLogMan);
        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(createSocketMan);
        TB_MESSAGES_DECLARE_MEMBER_FUNCTION(close);

        Core::ILogMan *    AllocateLogMan() override;
        Core::ISocketMan * AllocateSocketMan() override;

        void CreateLogMan();
        void CreateSocketMan();
    };

}
}