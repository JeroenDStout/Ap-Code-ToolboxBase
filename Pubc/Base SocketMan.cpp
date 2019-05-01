/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Version Reg.h"

#include "Conduits/Pubc/Disposable Message.h"
#include "Conduits/Pubc/Base Nexus.h"

#include "ToolboxBase/Pubc/Base Environment.h"
#include "ToolboxBase/Pubc/Base SocketMan.h"
#include "ToolboxBase/Pubc/Base SocketMan WSServer.h"
#include "ToolboxBase/Pubc/Interface LogMan.h"

using namespace Toolbox::Base;

    //  Relay message receiver
    // --------------------

CON_RMR_DEFINE_CLASS(Socketman);

    //  Setup
    // --------------------

Socketman::Socketman()
{
    namespace ph = std::placeholders;

    this->Unique_Reply_ID           = 1;
    this->Open_Conduit_Func         = std::bind(&Socketman::internal_async_open_conduit_func, this, ph::_1, ph::_2, ph::_3);
    this->Conduit_To_Socket_Func    = std::bind(&Socketman::internal_conduit_to_socket_func, this, ph::_1, ph::_2);
}

Socketman::~Socketman()
{
}

void Socketman::initialise(const JSON param)
{
    uint16_t port = 3001;   

        // Extract param
    const auto it = param.find("port");
    if (it != param.end()) {
        port = it->get<uint16_t>();
    }

        // Variations on localhost
    this->Whitelisted_Address_Test.push_back( { "localhost", std::regex("localhost") } );
    this->Whitelisted_Address_Test.push_back( { "localhost", std::regex("::1") } );
    this->Whitelisted_Address_Test.push_back( { "localhost", std::regex("::ffff:127\\.0\\.0\\.1") } );

        // Match 10.0.0.0 to 10.255.255.255
    this->Whitelisted_Address_Test.push_back( { "local network", std::regex("::ffff:10\\..*\\..*\\..*") } );

        // Match 172.16.0.0 to 172.31.255.255
    this->Whitelisted_Address_Test.push_back( { "local network", std::regex("::ffff:172\\.(1[6-9]|2[0-9]|3[01])\\..*\\..*") } );
    
        // Match 192.168.0.0 to 192.168.255.255
    this->Whitelisted_Address_Test.push_back( { "local network", std::regex("::ffff:192\\.168\\..*\\..*") } );

        // Create server
    this->Internal_WSServer = new WSServer();
    this->Internal_WSServer->socketman = this;
    
    this->Internal_WSServer->init_asio();
	this->Internal_WSServer->set_validate_handler(bind(&WSServer::on_validate, this->Internal_WSServer, std::placeholders::_1));
	this->Internal_WSServer->set_open_handler(bind(&WSServer::on_open, this->Internal_WSServer, std::placeholders::_1));
	this->Internal_WSServer->set_close_handler(bind(&WSServer::on_close, this->Internal_WSServer, std::placeholders::_1));
	this->Internal_WSServer->set_message_handler(bind(&WSServer::on_message, this->Internal_WSServer, std::placeholders::_1, std::placeholders::_2));
	this->Internal_WSServer->set_http_handler(bind(&WSServer::on_http, this->Internal_WSServer, std::placeholders::_1));
    
	this->Internal_WSServer->clear_access_channels(websocketpp::log::alevel::all); 

    this->Internal_WSServer->listen(port);

    this->Internal_WSServer->start_accept();

        // Create websocket listen threads
    for (int i = 0; i < 4; i++) {
        std::unique_ptr<std::thread> tmpPtr(new std::thread([=]{
            this->Internal_WSServer->run();
        }));
        this->WS_Listen_Threads.push_back(std::move(tmpPtr));
    }
    
        // Create nexus listen threads
    this->Should_Listen_To_Nexus = true;
    this->Message_Nexus->start_accepting_messages();
    this->Message_Nexus->start_accepting_threads();
    for (int i = 0; i < 1; i++) {
        std::unique_ptr<std::thread> tmpPtr(new std::thread([=]{
            while (this->Should_Listen_To_Nexus) {
                this->Message_Nexus->await_message_and_handle();
            }
        }));
        this->Nexus_Listen_Threads.push_back(std::move(tmpPtr));
    }
}

void Socketman::deinitialise_and_wait(const JSON param)
{
    using namespace std::chrono_literals;

        // Wait for nexus listening threads to end
    this->Should_Listen_To_Nexus = false;
    this->Message_Nexus->stop_accepting_threads_and_return();
    for (std::unique_ptr<std::thread> &t : this->Nexus_Listen_Threads) {
        t->join();
    }
    this->Nexus_Listen_Threads.clear();
    this->Message_Nexus->stop_gracefully();

        // Stop the WSServer after stopping the nexus
        // to ensure no stray messages hit the server
    this->Internal_WSServer->stop();

        // Wait for websocket listening threads to end
    for (std::unique_ptr<std::thread> &t : this->WS_Listen_Threads) {
        t->join();
    }
    this->WS_Listen_Threads.clear();

        // Delete server
    delete this->Internal_WSServer;
    this->Internal_WSServer = nullptr;
}

uint32 Socketman::get_unique_reply_id()
{
        // {paranoia} could theoretically overflow
        // if you send a thousand messages a second
        // for about three years
    return this->Unique_Reply_ID++;
}

    //  En passant nexus handling
    // --------------------

Conduits::Raw::INexus * Socketman::get_en_passant_nexus()
{
    return this->Message_Nexus;
}

void Socketman::connect_en_passant_conduit(Conduits::Raw::ConduitRef ref)
{
    std::shared_lock<std::shared_mutex> shlk(this->Mx_Nexus);
    this->Message_Nexus->manual_acknowledge_conduit(ref,
        [this](Conduits::Raw::ConduitRef, Conduits::ConduitUpdateInfo info) {
            std::shared_lock<std::shared_mutex> shlk(this->Mx_Nexus);
            this->En_Passant_Conduit = 0x0;
        },
        [](Conduits::Raw::ConduitRef, IMessage * msg) {
            std::unique_ptr<Conduits::DisposableMessage> reply(new Conduits::DisposableMessage());
            reply->Message_String = "Socketman does not allow talking back through the en passant circuit.";
            msg->set_response(reply.release());
            msg->set_FAILED();
            msg->release();
        }
    );
    this->En_Passant_Conduit = ref;
}

    //  Conduits
    // --------------------

bool Socketman::internal_async_open_conduit_func(Conduits::Raw::INexus * nexus, Conduits::BaseNexusMessage * msg, Conduits::Raw::IOpenConduitHandler * handler) noexcept
{
    Conduits::BaseNexus::InfoFunc info_func =
        [this](Conduits::Raw::ConduitRef ref, const Conduits::ConduitUpdateInfo info)
    {
        if (Conduits::ConduitState::is_closed(info.State)) {
            std::unique_lock<std::shared_mutex> lk(this->Mx_Conduit_To_Socket_Relay);
            this->Conduit_To_Socket_Map.erase(ref);
            return;
        }
    };
    Conduits::BaseNexus::MessageFunc message_func = this->Conduit_To_Socket_Func;

    Conduits::Raw::IOpenConduitHandler::ResultData data;

    auto id = this->Message_Nexus->manual_open_conduit_to(nexus, info_func, message_func);
    msg->Conduit_ID = id.first;

    handler->handle_success(&data, id.second);

    return true;
}

    //  Connexions
    // --------------------

bool Socketman::internal_check_connexion_is_allowed(std::string ip, std::string port)
{
    using cout = BlackRoot::Util::Cout;

    std::shared_lock<std::shared_mutex> read_lk(this->Mx_Whitelist);

        // First check if we already know this ip
    if (this->Whitelisted_Addresses.find(ip) != this->Whitelisted_Addresses.end())
        return true;

        // Check if we are allowed to be whitelisted
    bool allowed = false;
    std::string regName;
    for (auto & it : this->Whitelisted_Address_Test) {
        if (!std::regex_search(ip, it.second))
            continue;
        regName = it.first;
        allowed = true;
        break;
    }

    if (!allowed)
        return false;
    
        // Upgrade our lock; we need to write
    read_lk.unlock();
    std::unique_lock<std::shared_mutex> edit_lk(this->Mx_Whitelist);
    
        // Safety check if the ip was added by another thread
    if (this->Whitelisted_Addresses.find(ip) != this->Whitelisted_Addresses.end())
        return true;

        // Add address and cout{}
    this->Whitelisted_Addresses[ip] = regName;
    edit_lk.unlock();
    
    cout{} << "[SocketMan] New address connected : " << regName << " (" << ip << ")" << std::endl;

    return true;
}

void Socketman::internal_async_send_message(WSConnexionPtr sender, const std::string & payload)
{
    websocketpp::lib::error_code ec;
	this->Internal_WSServer->send(sender, payload, websocketpp::frame::opcode::binary, ec);
}

void Socketman::internal_async_receive_message(WSConnexionPtr sender, const std::string & payload)
{
    using cout = BlackRoot::Util::Cout;
        
	auto ptr = sender.lock();

        // See if we already know this connexion - we use a
        // shared lock so multiple incoming mesages can use
        // the web socket info
    std::shared_lock<std::shared_mutex> shlk(this->Mx_Web_Socket_Connexions);

        // Unknown connexion; it must be sending a what-ho message
    auto it = this->Web_Socket_Connexions.find(ptr);
    if (it == this->Web_Socket_Connexions.end()) {
        WSProp prop;

        try {
            auto client_prop = Conduits::Protocol::ClientProperties::try_parse_what_ho_message((void*)(payload.c_str()), payload.length());
            prop.Connexion_Established = std::chrono::system_clock::now();
            prop.Client_Prop = std::move(client_prop);
			    
            cout{} << "New websocket connexion:" << std::endl << " " << prop.Client_Prop.Name << std::endl << " " << prop.Client_Prop.Name << std::endl;
        }
		catch (std::exception e)
		{
			std::string str = "Invalid welcome message: ";
			str += e.what();
			this->internal_async_send_message(sender, str);
            return;
		}
		catch (BlackRoot::Debug::Exception * e)
		{
			std::string str = "Invalid welcome message: ";
			str += e->what();
			delete e;
			this->internal_async_send_message(sender, str);
            return;
		}
        catch (...)
        {
			std::string str = "Invalid welcome message!";
			this->internal_async_send_message(sender, str);
            return;
		}
        
            // Upgrade our lock to add the connexion
        shlk.unlock();

            // {PARANOIA} Quite possible that if lots of
            // messags are comign in, this unique lock will
            // not actually succeed for a while
        std::unique_lock<std::shared_mutex> unique_lk(this->Mx_Web_Socket_Connexions);

            // Sanity check; we could be here in 2 threads
            // if the client is so impatient it sends two
            // messages at once (it shouldn't)
	    auto & it_new = this->Web_Socket_Connexions.find(ptr);
        if (it_new != this->Web_Socket_Connexions.end()) {
			this->internal_async_send_message(sender, "We didn't even get to respond to your welcome! Please have patience.");
            return;
        }
        
            // Add us to the connexion list
        this->Web_Socket_Connexions[ptr] = std::move(prop);
        unique_lk.unlock();

            // Create server properties and send
		Conduits::Protocol::ServerProperties server_prop;
		server_prop.Name    = BlackRoot::Repo::VersionRegistry::GetMainProjectVersion().Name;
		server_prop.Version = BlackRoot::Repo::VersionRegistry::GetMainProjectVersion().Version;

		this->internal_async_send_message(sender, Conduits::Protocol::ServerProperties::create_what_ho_response(server_prop));

        return;
	}

        // If connexion is known, handle message

	try {
		auto intr = Conduits::Protocol::MessageScratch::try_parse_message((void*)(payload.c_str()), payload.length());

        this->internal_async_handle_message(it->first, it->second, intr);
	}
	catch (std::exception e)
	{
		std::string str = "Invalid message: ";
		str += e.what();
		this->internal_async_send_message(sender, str);
        return;
	}
	catch (BlackRoot::Debug::Exception * e)
	{
		std::string str = "Invalid message: ";
		str += e->what();
		delete e;
		this->internal_async_send_message(sender, str);
        return;
	}
    catch (...)
    {
		std::string str = "Invalid message!";
		this->internal_async_send_message(sender, str);
        return;
	}
}

void Socketman::internal_async_close_connexion(WSConnexionPtr sender)
{
    using cout = BlackRoot::Util::Cout;

    std::unique_lock<std::shared_mutex> shlk(this->Mx_Web_Socket_Connexions);

	auto ptr = sender.lock();
	
	auto & it = this->Web_Socket_Connexions.find(ptr);
	if (it != this->Web_Socket_Connexions.end()) {
		auto & prop = it->second;

		cout{} << "Closed connexion:" << std::endl << " " << prop.Client_Prop.Name << std::endl;

		this->Web_Socket_Connexions.erase(it);
	}

    shlk.unlock();

    this->internal_sync_remove_connexion(ptr);
}