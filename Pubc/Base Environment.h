/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <condition_variable>
#include <chrono>
#include <vector>
#include <atomic>

#include "BlackRoot/Pubc/JSON.h"

#include "Conduits/Pubc/Savvy Relay Receiver.h"
#include "Conduits/Pubc/Simple Relay Dir.h"
#include "Conduits/Pubc/Base Nexus.h"

#include "ToolboxBase/Pubc/Interface Environment.h"

namespace Toolbox {
namespace Base {

    class Socketman;
    class Logman;

    class BaseEnvironment : public Core::IEnvironment, public Conduits::SavvyRelayMessageReceiver {
        CON_RMR_DECLARE_CLASS(BaseEnvironment, SavvyRelayMessageReceiver);

        using JSON      = BlackRoot::Format::JSON;
        using FilePath  = BlackRoot::IO::FilePath;

    protected:
        struct StateType {
            typedef unsigned char Type;
            enum : Type {
                running       = 0,
                should_close  = 1,
                closed        = 2
            };
        };

        struct __EnvProps {
            FilePath     Boot_Dir;
            FilePath     Reference_Dir;
        } Env_Props;

        struct __BaseStats {
            std::chrono::high_resolution_clock::time_point  Start_Time;
        } Base_Stats;

        Conduits::NexusHolder<>       Message_Nexus;
        std::atomic<StateType::Type>  Current_State;

        Conduits::SimpleRelayDir      Simple_Relay;
        CON_RMR_USE_DIRECT_RELAY(Simple_Relay);
        
    protected:
            // Control
        
        virtual void internal_init_stats();

            // Typed

        virtual Core::ILogman *    internal_allocate_logman();
        virtual Core::ISocketman * internal_allocate_socketman();

    public:
        //virtual void InternalMessageLoop();
        //virtual void InternalHandleMessage(Messaging::IAsynchMessage*);

        //virtual void InternalSetupRelayMap();
        //
        //virtual void InternalInitStats();
        //virtual void InternalCompileStats(BlackRoot::Format::JSON &);
        
     //   virtual void internal_setup_relay_map();
        virtual void internal_compile_stats(JSON &);

        bool internal_get_thread_should_interrupt();

        //
        //void InternalMessageRelayToNone(std::string, Messaging::IAsynchMessage*);
        //void InternalMessageSendToNone(std::string, Messaging::IAsynchMessage*);

        //void InternalMessageSendToEnv(std::string, Messaging::IAsynchMessage*);
        
    public:
        BaseEnvironment();
        ~BaseEnvironment();
        
        //void ReceiveDelegateMessageAsync(Messaging::IAsynchMessage*) override;
        //void ReceiveDelegateMessageFromSocket(std::weak_ptr<void>, std::string) override;
        //void ReceiveDelegateMessageToSocket(std::weak_ptr<void>, Messaging::IAsynchMessage *) override;
        
            // Setup

        void set_boot_dir(FilePath) override;
        void set_ref_dir(FilePath) override;
        
            // Control

        void run_with_current_thread() override;
        void async_close() override;

        void create_logman();
        void create_socketman();

        virtual void internal_unload_all();
        
            // Util

        bool get_is_running() override;
        void async_receive_message(Conduits::Raw::IRelayMessage *) override;
        
            // Http

        void savvy_handle_http(const JSON httpRequest, JSON & httpReply, std::string & outBody);

            // Info

        FilePath get_ref_dir() override;
        
            // Relay Setup

        CON_RMR_DECLARE_FUNC(set_ref_dir);

            // Relay Data

        CON_RMR_DECLARE_FUNC(stats);
        CON_RMR_DECLARE_FUNC(code_credits);

            // Relay Control

        CON_RMR_DECLARE_FUNC(create_logman);
        CON_RMR_DECLARE_FUNC(create_socketman);
        CON_RMR_DECLARE_FUNC(close);

            // Relay Util

        CON_RMR_DECLARE_FUNC(ping);
    };

}
}