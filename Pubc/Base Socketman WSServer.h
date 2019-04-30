/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

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

#include "ToolboxBase/Pubc/Base Socketman.h"

class Toolbox::Base::Socketman::WSServer : public websocketpp::server<websocketpp::config::asio> {
public:
    using connexion = websocketpp::connection_hdl;

public:
    Socketman *socketman;
    
	void on_open(connexion hdl) {
	}

	void on_close(connexion hdl) {
		this->socketman->internal_async_close_connexion(hdl);
	}
    
    bool validate_via_socketman(connexion hdl) {
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

	bool on_validate(connexion hdl) {
        if (this->validate_via_socketman(hdl))
            return true;

            // If we can't validate this connexion,
            // let it at least know this is forbidden
		server::connection_ptr con = this->get_con_from_hdl(hdl);
        con->set_status(websocketpp::http::status_code::forbidden);
        con->set_body("HTTP/1.1 403 Forbidden\r\n\r\n\r\n");
        return false;
	}

	void on_message(connexion hdl, server::message_ptr msg) {
        using cout = BlackRoot::Util::Cout;
        
		server::connection_ptr con = this->get_con_from_hdl(hdl);
        if (!con)
            return;
		
        const auto & payload = msg->get_payload();
		cout{} << "msg: " << payload << std::endl;

		this->socketman->internal_async_receive_message(con, msg->get_payload());
	}

	void on_http(connexion hdl) {
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