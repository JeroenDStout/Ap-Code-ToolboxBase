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

#include "BlackRoot/Pubc/Threaded IO Stream.h"

#include "Conduits/Pubc/Function Message.h"

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
        // TODO

        //using cout = BlackRoot::Util::Cout;

        //std::string payload = msg->get_payload();

        //std::string prefixPlease = "please, ";

        //if (payload.substr(0, prefixPlease.size()) == prefixPlease) {
        //    if (!this->socketMan->InternalReceiveAdHocCommand(hdl, payload.substr(prefixPlease.size()))) {
        //        cout{} << "! Incoming 'please' message was not valid: " << msg->get_payload() << std::endl;
        //    }
        //}
        //else {
        //    cout{} << "! Incoming message was not valid: " << msg->get_payload() << std::endl;
        //}
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
        std::unique_lock<std::mutex> lk(this->Mx_Pending_Message_List);
        if (this->Sent_Pending_Message_List.size() == 0)
            break;
        lk.unlock();
        std::this_thread::sleep_for(100ms);
    }

        // Delete server
    delete this->Internal_Server;
    this->Internal_Server = nullptr;
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
            msg->set_response_string_with_copy("Socketman does not allow talking back through the en passant circuit.");
            msg->set_FAILED();
            msg->release();
        }
    );
    this->En_Passant_Conduit = ref;
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
        {   std::lock_guard<std::mutex> lk(this->Mx_Pending_Message_List);
            this->Sent_Pending_Message_List.erase(msg);
        }

        delete msg;
    });
    msg->Path = path;
    msg->Message_Segments[0] = header.dump();
    msg->sender_prepare_for_send();
    
        // Add to pending message list
    {   std::lock_guard<std::mutex> lk(this->Mx_Pending_Message_List);
        this->Sent_Pending_Message_List.insert(msg);
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

//bool SocketMan::InternalReceiveAdHocCommand(ConnectionPtr ptr, std::string string)
//{
//    Toolbox::Core::GetEnvironment()->ReceiveDelegateMessageFromSocket(ptr, string);
//    return true;
//}
//
//void SocketMan::MessageSendImmediate(std::weak_ptr<void> hdl, std::string string) 
//{
//    websocketpp::lib::error_code ec;
//
//    this->InternalServer->send(hdl, string, websocketpp::frame::opcode::text, ec);
//}