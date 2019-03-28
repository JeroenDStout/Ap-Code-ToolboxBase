#pragma warning(disable : 4996)

#define _SCL_SECURE_NO_WARNINGS
#define D_SCL_SECURE_NO_WARNINGS
#define ASIO_STANDALONE 
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_

#include <set>

#include "BlackRoot/Pubc/Threaded IO Stream.h"

#include "ToolboxBase/Pubc/Base Environment.h"
#include "ToolboxBase/Pubc/Base SocketMan.h"
#include "ToolboxBase/Pubc/Interface LogMan.h"

#include <asio.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/transport/stub/endpoint.hpp>

using namespace Toolbox::Base;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

class SocketMan::Server : public websocketpp::server<websocketpp::config::asio> {
public:
    SocketMan *socketMan;
    
	void on_open(connection_hdl hdl) {
	}

	void on_close(connection_hdl hdl) {
	}
    
    bool validate_via_socketman(connection_hdl hdl, std::string & str) {
        server::connection_ptr con = this->get_con_from_hdl(hdl);
        if (!con)
            return false;

        std::string endpoint = con->get_remote_endpoint();
        
        int brIndex0 = endpoint.find("[");
        int brIndex1 = endpoint.find("]");

        std::string early = endpoint.substr(brIndex0+1, brIndex1-1);
        std::string port  = endpoint.substr(brIndex1+1);

        return this->socketMan->InternalValidateConnection(early, port, str);
    }

	bool on_validate(connection_hdl hdl) {
        std::string name;

        if (this->validate_via_socketman(hdl, name))
            return true;

		server::connection_ptr con = this->get_con_from_hdl(hdl);
        con->set_status(websocketpp::http::status_code::forbidden);
        con->set_body("HTTP/1.1 403 Forbidden\r\n\r\n\r\n");
        return false;
	}

	void on_message(connection_hdl hdl, server::message_ptr msg) {
        using cout = BlackRoot::Util::Cout;

        std::string payload = msg->get_payload();

        std::string prefixPlease = "please, ";

        if (payload.substr(0, prefixPlease.size()) == prefixPlease) {
            if (!this->socketMan->InternalReceiveAdHocCommand(hdl, payload.substr(prefixPlease.size()))) {
                cout{} << "! Incoming 'please' message was not valid: " << msg->get_payload() << std::endl;
            }
        }
        else {
            cout{} << "! Incoming message was not valid: " << msg->get_payload() << std::endl;
        }
	}

	void on_http(connection_hdl hdl) {
        std::string dummy;
        
		server::connection_ptr con = this->get_con_from_hdl(hdl);
        if (!con)
            return;

        if (!this->validate_via_socketman(hdl, dummy)) {
            con->set_status(websocketpp::http::status_code::forbidden);
            return;
        }

        
        Toolbox::Messaging::AwaitMessage msg;
        msg.Message = { {  "env", { "Http" } } };
        
        Toolbox::Core::GetEnvironment()->ReceiveDelegateMessageAsync(&msg);
        if (msg.AwaitSuccess()) {
            Toolbox::Messaging::JSON resp = msg.Response["http"];
            con->set_body(resp.get<std::string>());
        }
        else {
            con->set_body(msg.Response.dump());
        }

		con->set_status(websocketpp::http::status_code::ok);
    }
};

void SocketMan::InternalServerListen()
{
    this->InternalServer->run();
}

void SocketMan::InternalServerTalk()
{

}

void SocketMan::Initialise(BlackRoot::Format::JSON * param)
{
    this->Whitelist.insert( { "::1", "localhost" } );
    this->Whitelist.insert( { "::ffff:127.0.0.1", "localhost" } );

    this->InternalServer = new Server;
    this->InternalServer->socketMan = this;
    
    this->InternalServer->init_asio();
	this->InternalServer->set_validate_handler(bind(&Server::on_validate, this->InternalServer, ::_1));
	this->InternalServer->set_open_handler(bind(&Server::on_open, this->InternalServer, ::_1));
	this->InternalServer->set_close_handler(bind(&Server::on_close, this->InternalServer, ::_1));
	this->InternalServer->set_message_handler(bind(&Server::on_message, this->InternalServer, ::_1, ::_2));
	this->InternalServer->set_http_handler(bind(&Server::on_http, this->InternalServer, ::_1));
    
	this->InternalServer->clear_access_channels(websocketpp::log::alevel::all); 

    this->InternalServer->listen(this->GetDefaultPort());
    this->InternalServer->start_accept();

    for (int i = 0; i < 8; i++) {
        std::unique_ptr<std::thread> tmpPtr(new std::thread(std::bind(&SocketMan::InternalServerListen, this)));
        this->ListenThreads.push_back(std::move(tmpPtr));
    }
}

void SocketMan::Deinitialise(BlackRoot::Format::JSON * param)
{
    this->InternalServer->stop();

    for (std::unique_ptr<std::thread> &t : this->ListenThreads) {
        t->join();
    }
    this->ListenThreads.clear();

    delete this->InternalServer;

    this->InternalServer = 0;
}

bool SocketMan::InternalValidateConnection(std::string ip, std::string port, std::string & outName)
{
    using cout = BlackRoot::Util::Cout;

    std::map<std::string, std::string>::iterator it;
    it = this->Whitelist.find(ip);
    if (it != this->Whitelist.end()) {
        cout{} << "[SocketMan] incoming : " << it->second << std::endl;
        outName = it->second;
        return true;
    }

    cout{} << "[SocketMan] Rejected: " << ip << std::endl;
    return false;
}

bool SocketMan::InternalReceiveAdHocCommand(ConnectionPtr ptr, std::string string)
{
    Toolbox::Core::GetEnvironment()->ReceiveDelegateMessageFromSocket(ptr, string);
    return true;
}

void SocketMan::MessageSendImmediate(std::weak_ptr<void> hdl, std::string string) 
{
    websocketpp::lib::error_code ec;

    this->InternalServer->send(hdl, string, websocketpp::frame::opcode::text, ec);
}