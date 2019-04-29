/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

/* TODO
 *  - handle situation in which socketman is deleted while a
      message is being handled
 */

#pragma warning(disable : 4996)

#define _SCL_SECURE_NO_WARNINGS
#define D_SCL_SECURE_NO_WARNINGS
#define ASIO_STANDALONE 
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_

#pragma warning(push)
#pragma warning(disable : 4267) // Conversion from 'size_t' to 'uint32_t', possible loss of data

#include <asio.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/transport/stub/endpoint.hpp>
#include <websocketpp/http/request.hpp>

#pragma warning(pop)

#include <chrono>

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Version Reg.h"

#include "Conduits/Pubc/Function Message.h"
#include "Conduits/Pubc/Disposable Message.h"

#include "ToolboxBase/Pubc/Base Environment.h"
#include "ToolboxBase/Pubc/Base SocketMan.h"
#include "ToolboxBase/Pubc/Interface LogMan.h"

using namespace Toolbox::Base;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

    //  Relay message receiver
    // --------------------

CON_RMR_DEFINE_CLASS(Socketman);

    //  Internal server
    // --------------------

class Socketman::Server : public websocketpp::server<websocketpp::config::asio> {
public:
    Socketman *socketman;
    
	void on_open(connection_hdl hdl) {
	}

	void on_close(connection_hdl hdl) {
		this->socketman->internal_async_close_connexion(hdl);
	}
    
    bool validate_via_socketman(connection_hdl hdl) {
        server::connection_ptr con = this->get_con_from_hdl(hdl);
        if (!con)
            return false;

        std::string endpoint = con->get_remote_endpoint();
        
        auto brIndex0 = endpoint.find("[");
        auto brIndex1 = endpoint.find("]");

        std::string early = endpoint.substr(brIndex0+1, brIndex1-1);
        std::string port  = endpoint.substr(brIndex1+1);

        return this->socketman->internal_check_connexion_is_allowed(early, port);
    }

	bool on_validate(connection_hdl hdl) {
        if (this->validate_via_socketman(hdl))
            return true;

		server::connection_ptr con = this->get_con_from_hdl(hdl);
        con->set_status(websocketpp::http::status_code::forbidden);
        con->set_body("HTTP/1.1 403 Forbidden\r\n\r\n\r\n");
        return false;
	}

	void on_message(connection_hdl hdl, server::message_ptr msg) {
        using cout = BlackRoot::Util::Cout;
        
		server::connection_ptr con = this->get_con_from_hdl(hdl);
        if (!con)
            return;
		
        const auto & payload = msg->get_payload();
		cout{} << "msg: " << payload << std::endl;

		this->socketman->internal_async_receive_message(con, msg->get_payload());
	}

	void on_http(connection_hdl hdl) {
        using cout = BlackRoot::Util::Cout;
        
		server::connection_ptr con = this->get_con_from_hdl(hdl);
        if (!con)
            return;

            // We check if this connexion should be allowed
        if (!this->validate_via_socketman(hdl)) {
            con->set_status(websocketpp::http::status_code::forbidden);
            con->set_body("HTTP/1.1 403 Forbidden\r\n\r\n\r\n");
            return;
        }

            // Process the HTTP request
        auto & request = con->get_request();
        websocketpp::http::parser::request parser;
        parser.consume(request.raw().c_str(), request.raw().length());

            // Turn the request into json
            // TODO
        JSON http_header = {};

            // Sanitise the uri so it fits with Socketman
        std::string uri = parser.get_uri();
        size_t remove;
        for (remove = 0; remove < uri.length(); remove++) {
            if (uri[remove] != '/')
                break;
        }
        uri.erase(0, remove);

        con->defer_http_response();

            // Let socketman handle it asynchronously
        this->socketman->internal_async_handle_http(uri, http_header,
                [con](JSON header, std::string body)
        {
            if (header.is_object()) {
                for (auto & it : header.items()) {
                    if (!it.value().is_string())
                        continue;
                    con->replace_header(it.key(), it.value());
                }
            }

            con->set_body(body);
            con->set_status(websocketpp::http::status_code::ok);
            con->send_http_response();
        });
    }
};

    //  Setup
    // --------------------

Socketman::Socketman()
{
    namespace ph = std::placeholders;

    this->Unique_Reply_ID           = 1;
    this->Open_Conduit_Func         = std::bind(&Socketman::internal_async_open_conduit_func, this, ph::_1, ph::_2, ph::_3);
    this->Send_On_From_Conduit_Func = std::bind(&Socketman::internal_send_on_from_conduit_func, this, ph::_1, ph::_2);
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
    this->Internal_Server = new Server;
    this->Internal_Server->socketman = this;
    
    this->Internal_Server->init_asio();
	this->Internal_Server->set_validate_handler(bind(&Server::on_validate, this->Internal_Server, ::_1));
	this->Internal_Server->set_open_handler(bind(&Server::on_open, this->Internal_Server, ::_1));
	this->Internal_Server->set_close_handler(bind(&Server::on_close, this->Internal_Server, ::_1));
	this->Internal_Server->set_message_handler(bind(&Server::on_message, this->Internal_Server, ::_1, ::_2));
	this->Internal_Server->set_http_handler(bind(&Server::on_http, this->Internal_Server, ::_1));
    
	this->Internal_Server->clear_access_channels(websocketpp::log::alevel::all); 

    this->Internal_Server->listen(port);

    this->Internal_Server->start_accept();

        // Create listen threads
    for (int i = 0; i < 8; i++) {
        std::unique_ptr<std::thread> tmpPtr(new std::thread([&]{
            this->Internal_Server->run();
        }));
        this->Listen_Threads.push_back(std::move(tmpPtr));
    }

        // TODO: runs forever
    this->Nexus_Listen_Thread.reset(new std::thread([&]{
        while (true) {
            this->Message_Nexus->await_message_and_handle();
        }
    }));
}

void Socketman::deinitialise_and_wait(const JSON param)
{
    using namespace std::chrono_literals;

    this->Internal_Server->stop();

        // Wait for listening threads to end
    for (std::unique_ptr<std::thread> &t : this->Listen_Threads) {
        t->join();
    }
    this->Listen_Threads.clear();

        // Wait for pending messages to finish, seeing as they
        // hold a pointer to us; for now we spinlock (TODO)
    while (true) {
        std::unique_lock<std::mutex> lk(this->Mx_Pending_Messages);
        if (this->Pending_Message_Map.size() == 0)
            break;
        lk.unlock();
        std::this_thread::sleep_for(100ms);
    }

        // Delete server
    delete this->Internal_Server;
    this->Internal_Server = nullptr;
}

uint32 Socketman::get_unique_reply_id()
{
        // {paranoia} could theoretically overflow
        // if you send a thousand messages a second
        // for about three years
    return this->Unique_Reply_ID++;
}

    //  Control
    // --------------------

Conduits::Raw::INexus * Socketman::get_en_passant_nexus()
{
    return this->Message_Nexus;
}

void Socketman::connect_en_passant_conduit(Conduits::Raw::ConduitRef ref)
{
    std::shared_lock<std::shared_mutex> shlk(this->Mx_Nexus);
    this->Message_Nexus->manual_acknowledge_conduit(ref,
        [=](Conduits::Raw::ConduitRef, Conduits::ConduitUpdateInfo info) {
        },
        [=](Conduits::Raw::ConduitRef, Conduits::Raw::IRelayMessage * msg) {
            std::string res = "Socketman does not allow talking back through the en passant circuit.";
            msg->set_response_string_with_copy(res.c_str());
            msg->set_FAILED();
            msg->release();
        }
    );
    this->En_Passant_Conduit = ref;
}

    //  Messages
    // --------------------

void Socketman::internal_async_close_connexion(ConnexionPtr sender)
{
    using cout = BlackRoot::Util::Cout;

    std::unique_lock<std::shared_mutex> shlk(this->Mx_Socket_Connexions);

	auto ptr = sender.lock();
	
	auto & it = this->Socket_Connexions.find(ptr);
	if (it != this->Socket_Connexions.end()) {
		auto & prop = it->second;

		cout{} << "Closed connexion:" << std::endl << " " << prop.Client_Prop.Name << std::endl;

		this->Socket_Connexions.erase(it);
	}
}

void Socketman::internal_async_receive_message(ConnexionPtr sender, const std::string & payload)
{
    using cout = BlackRoot::Util::Cout;

    std::shared_lock<std::shared_mutex> shlk(this->Mx_Socket_Connexions);

	auto ptr = sender.lock();
	auto & it = this->Socket_Connexions.find(ptr);
	if (it == this->Socket_Connexions.end()) {
		SockProp sock_prop;

		try {
			auto client_prop = Conduits::Protocol::ClientProperties::try_parse_what_ho_message((void*)(payload.c_str()), payload.length());

			sock_prop.Connexion_Established = std::chrono::system_clock::now();
			sock_prop.Client_Prop = std::move(client_prop);

			cout{} << "New websocket connexion:" << std::endl << " " << sock_prop.Client_Prop.Name << std::endl << " " << sock_prop.Client_Prop.Name << std::endl;
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
        
            // Upgrade our lock
        shlk.unlock();

            // {PARANOIA} Quite possible that if lots of
            // messags are comign in, this unique lock will
            // not actually succeed for a while
        std::unique_lock<std::shared_mutex> unique_lk(this->Mx_Socket_Connexions);

            // Sanity check; we could be here in 2 threads
            // if the client is so impatient it sends two
            // messages at once (it shouldn't)
	    auto & it_new = this->Socket_Connexions.find(ptr);
        if (it_new != this->Socket_Connexions.end()) {
			this->internal_async_send_message(sender, "We didn't even get to respond to your welcome! Please have patience.");
            return;
        }
        
        this->Socket_Connexions[ptr] = std::move(sock_prop);
        unique_lk.unlock();

		Conduits::Protocol::ServerProperties prop;
		prop.Name    = BlackRoot::Repo::VersionRegistry::GetMainProjectVersion().Name;
		prop.Version = BlackRoot::Repo::VersionRegistry::GetMainProjectVersion().Version;

		this->internal_async_send_message(sender, Conduits::Protocol::ServerProperties::create_what_ho_response(prop));

        return;
	}

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

void Socketman::internal_async_handle_message(ConnexionPtrShared sender, SockProp & prop, Conduits::Protocol::MessageScratch &intr)
{
    using cout = BlackRoot::Util::Cout;

        // This is being sent to nobody in particular;
        // route this to the en-passant
    if (intr.Recipient_ID == 0 && !intr.get_requires_response()) {
        auto * msg = new Conduits::DisposableMessage();
        msg->Path.assign(intr.String, intr.String_Length);
        msg->set_message_segments_from_list(intr.Segments);
        msg->sender_prepare_for_send();
                
        std::shared_lock<std::shared_mutex> shlk(this->Mx_Nexus);
        this->Message_Nexus->send_on(this->En_Passant_Conduit, msg);
        return;
    }

    if (intr.get_is_resonse()) {
            // TODO: all of this is iffy

        std::unique_lock<std::shared_mutex> lk(this->Mx_Pending_Reply);
        auto & it = this->Pending_Reply_Map.find(intr.Recipient_ID);
        if (it == this->Pending_Reply_Map.end()) {
                // TODO: handle
            cout{} << "Reply sent to id that was not used!";
        }
        auto * msg = it->second.Message_Waiting_For_Reply;
        this->Pending_Reply_Map.erase(it);
        lk.unlock();

        if (intr.String_Length > 0) {
                // we really just use std string
                // to add a zero; it's very decadent
            std::string str;
            str.assign(intr.String, intr.String_Length);
            msg->set_response_string_with_copy(str.c_str());
        }
        for (auto & elem : intr.Segments) {
            msg->set_response_segment_with_copy(elem.first, elem.second);
        }
        if (intr.get_has_succeeded()) {
            msg->set_OK();
            if (intr.get_confirm_open_conduit()) {
                msg->set_OK_opened_conduit();
            }
        }
        else {
            if (intr.get_connexion_failure()) {
                msg->set_FAILED_connexion();
            }
            else {
                msg->set_FAILED();
            }
        }
        
        msg->release();
        return;
    }

    SendOnFromConduitProp send_on_prop;
    send_on_prop.Connexion    = sender;
    send_on_prop.Receiver_ID  = intr.Reply_To_Me_ID;

        // Create a message which will send back the response
        // We set reply_to_me to 0 as we do not expect a reply
    auto * msg = new Conduits::BaseFunctionMessage([=](Conduits::BaseFunctionMessage * msg) {
        Conduits::Protocol::MessageScratch res;

            // Sanity clip on response string - way more than
            // should in any sane way be used
        if (msg->Response_String.size() > 0xFFFF) {
            msg->Response_String.resize(0xFFFF);
        }

        res.Recipient_ID           = send_on_prop.Receiver_ID;
        res.Reply_To_Me_ID         = 0;

        res.set_is_response(true);

        switch (msg->Message_State) {
        case Conduits::MessageState::ok:
            res.set_has_succeeded(true);
            break;
        case Conduits::MessageState::ok_opened_conduit:
            res.set_has_succeeded(true);
            res.set_confirm_open_conduit(true);
            break;
        case Conduits::MessageState::connexion_failure:
            res.set_connexion_failure(true);
            res.set_has_succeeded(false);
            break;
        default:
            res.set_has_succeeded(false);
            break;
        }

        if (msg->Message_State == Conduits::MessageState::ok_opened_conduit) {
            res.set_confirm_open_conduit(true);
            res.set_has_succeeded(true);
        }
        else if (msg->Message_State == Conduits::MessageState::ok) {
            res.set_has_succeeded(true);
        }
        else {
            res.set_has_succeeded(false);
        }

        res.String          = msg->Response_String.c_str();
        res.String_Length   = (uint16)msg->Response_String.size();

        for (auto & it : msg->Response_Segments) {
            Conduits::Raw::SegmentData dat;
            dat.Data = (void*)it.second.data();
            dat.Length = it.second.size();
            res.Segments.push_back({ it.first, dat });
        }
            // Remove us from the pending message list
        {   std::lock_guard<std::mutex> lk(this->Mx_Pending_Messages);
            auto it = this->Pending_Message_Map.find(msg);
            res.Reply_To_Me_ID = it->second.Reply_To_Me_ID;
        }

        if (res.get_confirm_open_conduit()) {
            std::lock_guard<std::shared_mutex> lk(this->Mx_Send_On_From_Conduit);
            this->Send_On_From_Conduit_Map[res.Reply_To_Me_ID] = send_on_prop;
        }
        
		this->internal_async_send_message(sender, Conduits::Protocol::MessageScratch::try_stringify_message(res));
        
            // Remove us from the pending message list
        {   std::lock_guard<std::mutex> lk(this->Mx_Pending_Messages);
            this->Pending_Message_Map.erase(msg);
        }

        delete msg;
    });

    msg->Path.assign(intr.String, intr.String_Length);

    msg->set_message_segments_from_list(intr.Segments);
    msg->set_open_conduit_function(&this->Open_Conduit_Func);
    
        // Add to pending message list
    {   std::lock_guard<std::mutex> lk(this->Mx_Pending_Messages);
        this->Pending_Message_Map[msg] = { 0 };
    }

    msg->sender_prepare_for_send();

    if (intr.Recipient_ID == 0) {    
        std::shared_lock<std::shared_mutex> shlk(this->Mx_Nexus);
        this->Message_Nexus->send_on(this->En_Passant_Conduit, msg);
    }
    else {
        std::shared_lock<std::shared_mutex> shlk(this->Mx_Nexus);
        this->Message_Nexus->send_on(intr.Recipient_ID, msg);
    }
}

void Socketman::internal_send_on_from_conduit_func(Conduits::Raw::ConduitRef ref, Conduits::Raw::IRelayMessage *msg)
{
    std::shared_lock<std::shared_mutex> lk(this->Mx_Send_On_From_Conduit);
    auto & it = this->Send_On_From_Conduit_Map.find(ref);
    if (it == this->Send_On_From_Conduit_Map.end()) {
        msg->set_FAILED_connexion();
        msg->release();
        return;
    }
    auto connexion   = it->second.Connexion;
    auto receiver_id = it->second.Receiver_ID;
    lk.unlock();

    size_t path_str_len = strlen(msg->get_path_string());
    DbAssertFatal(path_str_len < 0xFFFF);

    Conduits::Protocol::MessageScratch intr;
    intr.String = msg->get_path_string();
    intr.String_Length = uint16(path_str_len);
    intr.Recipient_ID  = receiver_id;

    intr.set_requires_response(msg->get_response_expectation() == Conduits::ResponseDesire::needed);
    if (intr.get_requires_response()) {
        intr.Reply_To_Me_ID = this->get_unique_reply_id();

        PendingReplyProp prop;
        prop.Message_Waiting_For_Reply = msg;

        std::unique_lock<std::shared_mutex> lk(this->Mx_Pending_Reply);
        this->Pending_Reply_Map[intr.Reply_To_Me_ID] = prop;
    }

    uint8 usedSegments[256];
    uint8 indexCount = msg->get_message_segment_indices(usedSegments, 256);

    for (int i = 0; i < indexCount; i++) {
        auto seg = msg->get_message_segment(usedSegments[i]);

        Conduits::Protocol::MessageScratch::SegmentElem elem;
        elem.first  = usedSegments[i];
        elem.second.Data   = seg.Data;
        elem.second.Length = seg.Length;
        intr.Segments.push_back(elem);
    }

    this->internal_async_send_message(connexion, Conduits::Protocol::MessageScratch::try_stringify_message(intr));
}

void Socketman::internal_async_send_message(ConnexionPtr sender, const std::string & payload)
{
    websocketpp::lib::error_code ec;
	this->Internal_Server->send(sender, payload, websocketpp::frame::opcode::binary, ec);
}

    //  Conduits
    // --------------------

bool Socketman::internal_async_open_conduit_func(Conduits::Raw::INexus * nexus, Conduits::Raw::IRelayMessage * msg, Conduits::Raw::IOpenConduitHandler * handler) noexcept
{
    std::shared_lock<std::shared_mutex> shlk(this->Mx_Nexus);
    
    Conduits::BaseNexus::InfoFunc info_func =
        [this](Conduits::Raw::ConduitRef, const Conduits::ConduitUpdateInfo)
    {
    };
    Conduits::BaseNexus::MessageFunc message_func = this->Send_On_From_Conduit_Func;

    Conduits::Raw::IOpenConduitHandler::ResultData data;

    auto id = this->Message_Nexus->manual_open_conduit_to(nexus, info_func, message_func);
    shlk.unlock();
    
    std::unique_lock<std::mutex> lk(this->Mx_Pending_Messages);
    this->Pending_Message_Map[msg].Reply_To_Me_ID = id.first;
    lk.unlock();

    handler->handle_success(&data, id.second);

    return true;
}

    //  Http
    // --------------------

void Socketman::internal_async_handle_http(std::string path, JSON header, HandleHttpCallback callback)
{
        // Strip preceeding / if needed
    if (path.length() > 0 && path[0] == '/') {
        path.erase(0, 1);
    }

        // If the url contains a query, we strip it off and add it
        // to the http header info
    auto q_index = path.find('?');
    if (q_index != path.npos) {
        std::string query = path.substr(q_index+1, path.length() - q_index - 1);
        std::string token, value;
        path.erase(path.begin() + q_index, path.end());

        int queryCount = 0;

        JSON & h_query = header["query"];
            
        bool last = false;
        while (true) {
            auto sep_elem  = query.find("&");
            auto sep_value = query.find("=");

            if (sep_elem == query.npos) {
                sep_elem = query.length();
                last = true;
            }

                // See if this query element has a value
            if (sep_value < sep_elem) {
                token = query.substr(0, sep_value);
                h_query[token] = query.substr(sep_value+1, sep_elem);
                if (last)
                    break;
                query.erase(0, sep_elem+1);
                continue;
            }
            
            token = query.substr(0, sep_elem);
            h_query[token] = true;

            if (last)
                break;
            
            query.erase(0, sep_elem+1);
        }
    }

        // We add 'http' to the path as internally 'http' really is
        // actually a function name; when the path is meant to lead
        // to an actual file (i.e., iris/web/rich_evans_slips.gif),
        // the file handler will have to accept there being an extra
        // http at the end
    if (path.length() > 0 && path[path.length()-1] != '/') {
        path += '/';
    }
    path += "http";

        // Create a message which will handle the http response
        // upon its return
    auto * msg = new Conduits::BaseFunctionMessage([=](Conduits::BaseFunctionMessage * msg) {
        if (msg->Message_State == Conduits::MessageState::ok) {
            JSON header = JSON(nullptr);

                // Load the headers, if set
            auto & headerIt = msg->Response_Segments.find(0);
            if (headerIt != msg->Response_Segments.end()) {
                try {
                    header = JSON::parse(headerIt->second);
                }
                catch (...)
                { ; }
            }
            
                // If we have a body, send it; otherwise send
                // a dummy body ~ though http responses really
                // generally should contain a body
            auto & bodyIt = msg->Response_Segments.find(1);
            if (bodyIt != msg->Response_Segments.end()) {
                callback(header, bodyIt->second);
            }
            else {
                callback(header, "this space intentionally left blank");
            }
        }
        else {
                // Our message failed; dump info
            std::stringstream err;
            err << "<b>This request could not be completed</b>";
            if (msg->Message_State == Conduits::MessageState::connexion_failure) {
                err << "<br/>Conduit connexion failed";
            }
            if (msg->Response_String.length() > 0) {
                err << "<br/>" << msg->Response_String;
            }
            for (auto & it : msg->Response_Segments) {
                err << "<br/><i>Segment " << it.first << "</i>: ";
                err << it.second;
            }
            callback({}, err.str());
        }
        
            // Remove us from the pending message list
        {   std::lock_guard<std::mutex> lk(this->Mx_Pending_Messages);
            this->Pending_Message_Map.erase(msg);
        }

        delete msg;
    });
    msg->Path = path;
    msg->Message_Segments[0] = header.dump();
    msg->sender_prepare_for_send();
    
        // Add to pending message list
    {   std::lock_guard<std::mutex> lk(this->Mx_Pending_Messages);
        this->Pending_Message_Map[msg] = {};
    }

        // Send message off
    std::shared_lock<std::shared_mutex> shlk(this->Mx_Nexus);
    this->Message_Nexus->send_on(this->En_Passant_Conduit, msg);
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