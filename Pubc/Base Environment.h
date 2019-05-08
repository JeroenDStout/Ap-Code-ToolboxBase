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
            FilePath     User_Dir;
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
        virtual void internal_compile_stats(JSON &);

        bool internal_get_thread_should_interrupt();
        
            // Web
        
        virtual std::string internal_get_favicon_name() { return ""; }
        virtual void internal_handle_web_request(std::string path, Conduits::Raw::IMessage *);
        
    public:
        BaseEnvironment();
        ~BaseEnvironment();
        
            // Setup

        void set_boot_dir(FilePath) override;
        void set_ref_dir(FilePath) override;
        void set_user_dir(FilePath) override;
        
            // Control

        void run_with_current_thread() override;
        void async_close() override;

        virtual void create_logman(const JSON);
        virtual void create_socketman(const JSON);

        virtual void internal_unload_all();
        
            // Util

        bool get_is_running() override;
        void async_receive_message(Conduits::Raw::IMessage *) override;

        virtual FilePath expand_dir(FilePath) override;
        
            // Http

        void savvy_handle_http(const JSON httpRequest, JSON & httpReply, std::string & outBody);

            // Info

        FilePath get_ref_dir() override;
        FilePath get_user_dir() override;
        
            // Relay Setup

        CON_RMR_DECLARE_FUNC(set_ref_dir);
        CON_RMR_DECLARE_FUNC(set_user_dir);

            // Relay Data

        CON_RMR_DECLARE_FUNC(stats);
        CON_RMR_DECLARE_FUNC(code_credits);

            // Relay Control

        CON_RMR_DECLARE_FUNC(create_logman);
        CON_RMR_DECLARE_FUNC(create_socketman);
        CON_RMR_DECLARE_FUNC(close);

            // Relay Util

        CON_RMR_DECLARE_FUNC(ping);
        CON_RMR_DECLARE_FUNC(beep);
    };

}
}