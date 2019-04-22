/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <thread>
#include <regex>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <map>
#include <set>
#include <chrono>

#include "Conduits/Pubc/Interface Nexus.h"
#include "Conduits/Pubc/Savvy Relay Receiver.h"
#include "Conduits/Pubc/Websocket Protocol.h"

#include "ToolboxBase/Pubc/Interface Socketman.h"

namespace Toolbox {
namespace Base {
    
	class Socketman : public Toolbox::Core::ISocketman, public Conduits::SavvyRelayMessageReceiver {
        CON_RMR_DECLARE_CLASS(Socketman, SavvyRelayMessageReceiver);

    public:
        using JSON                  = BlackRoot::Format::JSON;
        using HandleHttpCallback    = std::function<void(JSON header, std::string body)>;
		using ConnexionPtr		    = std::weak_ptr<void>;
		using ConnexionPtrShared    = std::shared_ptr<void>;

		struct SocketConnexionProperties {
			std::chrono::system_clock::time_point  Connexion_Established;
			Conduits::Protocol::ClientProperties   Client_Prop;
		};
        using SockProp = SocketConnexionProperties;

	protected:
        class Server;
        friend Server;

        using ListenThreadRef    = std::unique_ptr<std::thread>;
        
        using WhitelistTest      = std::pair<std::string, std::regex>;
        using WhitelistMap       = std::map<std::string, std::string>;

        using RlMessage          = Conduits::Raw::IRelayMessage;

        using SockMap            = std::map<ConnexionPtrShared, SockProp>;

        Server  *Internal_Server;
		
        std::shared_mutex          Mx_Socket_Connexions;
		SockMap					   Socket_Connexions;

        std::shared_mutex          Mx_Nexus;
        Conduits::NexusHolder<>    Message_Nexus;
        Conduits::Raw::ConduitRef  En_Passant_Conduit;

        std::vector<ListenThreadRef>    Listen_Threads;
        
        std::shared_mutex          Mx_Whitelist;
        std::vector<WhitelistTest> Whitelisted_Address_Test;
        WhitelistMap               Whitelisted_Addresses;

        std::mutex                 Mx_Pending_Message_List;
        std::set<RlMessage*>       Sent_Pending_Message_List;

        //virtual void internal_server_listen();
        //virtual void internal_server_talk();

        virtual bool internal_check_connexion_is_allowed(std::string ip, std::string port);
        virtual void internal_async_handle_http(std::string path, JSON header, HandleHttpCallback);
		
		virtual void internal_async_close_connexion(ConnexionPtr sender);
		virtual void internal_async_receive_message(ConnexionPtr sender, const std::string & payload);
		virtual void internal_async_send_message(ConnexionPtr sender, const std::string & payload);

        //virtual bool internal_receive_ad_hoc_command(ConnexionPtr, std::string);
    public:
        Socketman();
        ~Socketman() override;

        void    initialise(const JSON param) override;
        void    deinitialise_and_wait(const JSON param) override;

        Conduits::Raw::INexus * get_en_passant_nexus() override;
        void    connect_en_passant_conduit(Conduits::Raw::ConduitRef) override;

        //virtual int GetDefaultPort() { return 3001; }
	};

}
}