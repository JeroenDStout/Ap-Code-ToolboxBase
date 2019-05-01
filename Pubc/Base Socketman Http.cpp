/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

 #include <iomanip>

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"

#include "Conduits/Pubc/Disposable Message.h"

#include "ToolboxBase/Pubc/Base SocketMan.h"

using namespace Toolbox::Base;

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
    auto * msg = new Conduits::BaseNexusMessage();
    msg->On_Release_Func = [=](Conduits::BaseNexusMessage * msg) {
        if (msg->result_is_OK() && msg->Response) {
            auto _header = msg->Response->get_message_segment("header");
            auto _body   = msg->Response->get_message_segment("body");

            JSON header = JSON(nullptr);

            try {
                DbAssertFatal(_header.Data);
                header = JSON::parse(std::basic_string_view<char>((char*)_header.Data, _header.Length));
            }
            catch (...)
            { ; }

            if (_body.Data) {
                callback(header, std::string((char*)_body.Data, _body.Length));
            }
            else {
                callback(header, "This space intentionally left blank");
            }

            msg->Response->set_OK();
            msg->Response->release();
            return;
        }
        
            // Our message failed; dump info
        std::stringstream err;
        err << "<b>This request could not be completed</b>";

        switch (msg->State_Fail) {
            default:
                err << "<br/>Unexpected error (0x" << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << msg->State_Fail << ")";
                break;

            case Conduits::Raw::FailState::failed:
                break;

            case Conduits::Raw::FailState::failed_no_conduit:
                err << "<br/>No conduit";
                break;

            case Conduits::Raw::FailState::failed_no_connexion:
                err << "<br/>No connexion";
                break;

            case Conduits::Raw::FailState::failed_timed_out:
                err << "<br/>Timed out";
                break;
        }

        if (msg->Response) {
            auto * string = msg->Response->get_message_string();
            
            if (string && string[0]) {
                err << "<br/>" << string;
            }

            auto segment_count = msg->get_message_segment_count();
            if (segment_count > 0) {
                std::vector<Conduits::Raw::SegmentData> seg_data(segment_count);
                msg->get_message_segment_list(seg_data.data(), segment_count);

                for (auto & it : seg_data) {
                    err << "<br/><i>Segment " << it.Name << "</i>: ";
                    err.write((char*)it.Data, it.Length);
                }
            }
            
            msg->Response->set_OK();
            msg->Response->release();
        }
        callback({}, err.str());
    };
    msg->set_message_string_as_path(path);
    msg->Response_Desire = Conduits::Raw::ResponseDesire::required;
    msg->Segment_Map["header"] = header.dump();

    msg->sender_prepare_for_send();

        // Send message off
    std::shared_lock<std::shared_mutex> shlk(this->Mx_Nexus);
    this->Message_Nexus->setup_use_queue_for_release_event(msg);
    this->Message_Nexus->send_on_conduit(this->En_Passant_Conduit, msg);
}