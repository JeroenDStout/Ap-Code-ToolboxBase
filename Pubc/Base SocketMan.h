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
#include "Conduits/Pubc/Base Message.h"
#include "Conduits/Pubc/Savvy Relay Receiver.h"
#include "Conduits/Pubc/Base Nexus.h"
#include "Conduits/Pubc/Base Nexus Message.h"
#include "Conduits/Pubc/Websocket Protocol What-ho.h"
#include "Conduits/Pubc/Websocket Protocol Messages.h"

#include "ToolboxBase/Pubc/Interface Socketman.h"

namespace Toolbox {
namespace Base {
    
	class Socketman : public Toolbox::Core::ISocketman, public Conduits::SavvyRelayMessageReceiver {
        CON_RMR_DECLARE_CLASS(Socketman, SavvyRelayMessageReceiver);

    public:
        using JSON                  = BlackRoot::Format::JSON;
        using HandleHttpCallback    = std::function<void(JSON header, std::string body)>;

        using ConduitRef            = Conduits::Raw::ConduitRef;
        using IMessage              = Conduits::Raw::IMessage;
        using INexus                = Conduits::Raw::INexus;

        using OpenConduitFunc       = Conduits::BaseNexusMessage::OpenConduitFunc;
        using SendOnFromConduitFunc = std::function<void(Conduits::Raw::ConduitRef, IMessage*)>;

		using WSConnexionPtr		= std::weak_ptr<void>;
		using WSConnexionPtrShared  = std::shared_ptr<void>;

		struct WebSocketConnexionProperties {
			std::chrono::system_clock::time_point  Connexion_Established;
			Conduits::Protocol::ClientProperties   Client_Prop;
		};
        using WSProp = WebSocketConnexionProperties;

	protected:
            // Declared in cpp, internal server

        class WSServer;
        friend WSServer;
        WSServer * Internal_WSServer;

            // Running threads

        using ListenThreadRef    = std::unique_ptr<std::thread>;
        
        std::vector<ListenThreadRef> WS_Listen_Threads;

            // Whitelisting IPs and addresses

        using WhitelistTest      = std::pair<std::string, std::regex>;
        using WhitelistMap       = std::map<std::string, std::string>;
        
        std::shared_mutex          Mx_Whitelist;
        std::vector<WhitelistTest> Whitelisted_Address_Test;
        WhitelistMap               Whitelisted_Addresses;

            // Messages sent away, waiting reply
            
        struct SentAwayPendingReplyProp {
            WSConnexionPtrShared    Connexion;
            IMessage                *Message_Waiting_For_Reply;
        };
        using PendingReplyMap = std::map<uint32, SentAwayPendingReplyProp>;
        
        std::shared_mutex       Mx_Pending_Reply;
        PendingReplyMap         Pending_Reply_Map;

            // Handling sockets

        using WSMap = std::map<WSConnexionPtrShared, WSProp>;
        
        std::shared_mutex       Mx_Web_Socket_Connexions;
		WSMap			        Web_Socket_Connexions;

            // Handling nexus
		
        std::shared_mutex       Mx_Nexus;
        Conduits::NexusHolder<> Message_Nexus;
        ConduitRef              En_Passant_Conduit;
        OpenConduitFunc         Open_Conduit_Func;
        
        struct ConduitToSocketProp {
            WSConnexionPtrShared    Connexion;
            uint32                  Receiver_ID;
        };
        using ConduitToSocketMap    = std::map<ConduitRef, ConduitToSocketProp>;

        std::shared_mutex       Mx_Conduit_To_Socket_Relay;
        SendOnFromConduitFunc   Conduit_To_Socket_Func;
        ConduitToSocketMap      Conduit_To_Socket_Map;
        
        bool                         Should_Listen_To_Nexus;
        std::vector<ListenThreadRef> Nexus_Listen_Threads;

            // IDs
        
        std::atomic<uint32>        Unique_Reply_ID;

        uint32  get_unique_reply_id();
        
            // Handling

        virtual bool internal_check_connexion_is_allowed(std::string ip, std::string port);
        virtual void internal_async_handle_http(std::string path, JSON header, HandleHttpCallback);
		virtual void internal_async_close_connexion(WSConnexionPtr sender);
		virtual void internal_async_receive_message(WSConnexionPtr sender, const std::string & payload);
		virtual void internal_async_handle_message(WSConnexionPtrShared, WSProp & prop, Conduits::Protocol::MessageScratch &);
		virtual void internal_async_send_message(WSConnexionPtr sender, const std::string & payload);

		virtual uint32 internal_async_add_pending_message(WSConnexionPtr sender, IMessage*);
		
            // Conduits

        virtual bool internal_async_open_conduit_func(INexus *, Conduits::BaseNexusMessage *, Conduits::Raw::IOpenConduitHandler *) noexcept;
        virtual void internal_conduit_to_socket_func(ConduitRef, IMessage*);

        virtual void internal_sync_remove_connexion(WSConnexionPtrShared);

    public:
        Socketman();
        ~Socketman() override;

            // Implement ISocketman

        void    initialise(const JSON param) override;
        void    deinitialise_and_wait(const JSON param) override;

        Conduits::Raw::INexus * get_en_passant_nexus() override;
        void    connect_en_passant_conduit(Conduits::Raw::ConduitRef) override;
	};

}
}