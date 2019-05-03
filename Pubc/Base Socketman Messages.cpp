/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"

#include "Conduits/Pubc/Disposable Message.h"

#include "ToolboxBase/Pubc/Base Environment.h"
#include "ToolboxBase/Pubc/Base SocketMan.h"
#include "ToolboxBase/Pubc/Base SocketMan WSServer.h"
#include "ToolboxBase/Pubc/Interface LogMan.h"

using namespace Toolbox::Base;

    //  Messages
    // --------------------

void Socketman::internal_async_handle_message(WSConnexionPtrShared sender, WSProp & prop, Conduits::Protocol::MessageScratch &intr)
{
    using cout = BlackRoot::Util::Cout;

#if SOCKETMAN_PARANOIA
    {   std::stringstream ss;
        ss << std::endl << "Msg:";
        if (intr.get_is_response()) {
            if (intr.get_is_OK()) {
                ss << " V";
            }
            else {
                ss << " X";
            }
            ss << " @" << intr.Recipient_ID;
        }
        else {
            ss << " $" << intr.Recipient_ID;
        }
        if (intr.get_accepts_response()) {
            ss << " %" << intr.Reply_To_Me_ID;
        }
        if (intr.get_confirm_open_conduit()) {
            ss << " #" << intr.Opened_Conduit_ID;
        }
        if (intr.String_Length > 0) {
            ss << " >" << intr.String;
        }
        for (auto & seg : intr.Segments) {
            ss << std::endl << " * '" << seg.Name << "': ";
            if (seg.Length < 40*5) {
                ss.write((char*)seg.Data, seg.Length);
            }
            else {
                ss.write((char*)seg.Data, 40*5);
                ss << "...";
            }
        }
        cout{} << ss.str() << std::endl;
    }
#endif

        // If the recipient ID is 0 and the connexion
        // does not require a response just take the easy
        // option and route it as a disposable message
    if (intr.Recipient_ID == 0 && !intr.get_accepts_response()) {
        std::unique_ptr<Conduits::DisposableMessage> msg(new Conduits::DisposableMessage());

            // We set the string as a path as 'unsollicited' messages
            // delivered en passant are always paths and we want to give
            // some lenience to dirtbag scripts
        msg->set_message_string_as_path(std::string(intr.String, intr.String_Length));
        msg->add_message_segments_from_list(intr.Segments);

        msg->sender_prepare_for_send();
        
        std::shared_lock<std::shared_mutex> shlk(this->Mx_Nexus);
        this->Message_Nexus->send_on_conduit(this->En_Passant_Conduit, msg.release());
    }

    Conduits::Raw::IMessage * original_msg = nullptr;

        // If the mesage is a response we connect it with the
        // reply that has been waiting for it; if the reply does
        // not expect a reply we can instantly send it
    if (intr.get_is_response()) {
        std::unique_lock<std::shared_mutex> lk(this->Mx_Pending_Reply);
        
        auto & it = this->Pending_Reply_Map.find(intr.Recipient_ID);
        if (it == this->Pending_Reply_Map.end()) {
            
                // Unlock and (if allowed) send an error reply
            lk.unlock();
            if (intr.get_accepts_response()) {
                Conduits::Protocol::MessageScratch reply;
                reply.set_is_response(true);
                reply.set_is_OK(false);
                reply.set_connexion_failure(true);
                reply.set_accepts_response(false);

                std::stringstream ss;
                ss << "The id 0x" << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << intr.Recipient_ID << " is not a known reply id.";
                std::string str = ss.str();

                reply.String = str.c_str();
                reply.String_Length = (uint16)str.length();
                
		        this->internal_async_send_message(sender, Conduits::Protocol::MessageScratch::try_stringify_message(reply));
            }

            return;
        }

        original_msg = it->second.Message_Waiting_For_Reply;
        this->Pending_Reply_Map.erase(it);
        lk.unlock();

            // Transfer our incoming state to the original message
        if (intr.get_is_OK()) {
            if (intr.get_confirm_open_conduit()) {
                original_msg->set_OK(Conduits::Raw::OKState::ok_opened_conduit);
            }
            else {
                original_msg->set_OK();
            }
        }
        else {
            if (intr.get_connexion_failure()) {
                original_msg->set_FAILED(Conduits::Raw::FailState::failed_no_connexion);
            }
            else if (intr.get_no_response_expected()) {
                original_msg->set_FAILED(Conduits::Raw::FailState::failed_no_response_expected);
            }
            else if (intr.get_timed_out()) {
                original_msg->set_FAILED(Conduits::Raw::FailState::failed_timed_out);
            }
            else if (intr.get_receiver_will_not_handle()) {
                original_msg->set_FAILED(Conduits::Raw::FailState::receiver_will_not_handle);
            }
            else {
                original_msg->set_FAILED();
            }
        }

            // An empty message really is just a success/failure indication,
            // so this suffices and we can release the message
        if (intr.String_Length == 0 && intr.Segments.size() == 0) {
            original_msg->release();
            return;
        }

            // If a reply does not accept a response we route it via
            // as a disposable message
        if (!intr.get_accepts_response()) {
            std::unique_ptr<Conduits::DisposableMessage> reply(new Conduits::DisposableMessage());

            reply->Message_String.assign(intr.String, intr.String_Length);
            reply->add_message_segments_from_list(intr.Segments);

            reply->sender_prepare_for_send();
        
            original_msg->set_response(reply.release());
            original_msg->release();
            return;
        }

        // ↵ We do not return here; the incoming message had info
        //  which we need to add as a response proper
    }

    auto * msg = new Conduits::BaseNexusMessage();
    if (intr.get_accepts_response()) {
        msg->Response_Desire = Conduits::Raw::ResponseDesire::required;
    }

    auto reply_to_me = intr.Reply_To_Me_ID;
    auto connexion   = sender;

        // Our release function will cause the message, once handled
        // to 'swoop back' (technical jargon) to here, where it can
        // be sent back over the websocket
    msg->On_Release_Func = [=](Conduits::BaseNexusMessage * msg) {
        Conduits::Protocol::MessageScratch reply;
        reply.Recipient_ID = reply_to_me;
        reply.set_is_response(true);
        reply.set_is_OK(msg->result_is_OK());

            // Transfer the state of the finished message
            // onto the scratch object
        if (msg->result_is_OK()) {
            if (msg->State_OK == Conduits::Raw::OKState::ok_opened_conduit) {
                reply.set_confirm_open_conduit(true);
                reply.Opened_Conduit_ID = msg->Conduit_ID;

                std::unique_lock<std::shared_mutex> lk(this->Mx_Conduit_To_Socket_Relay);
                this->Conduit_To_Socket_Map[msg->Conduit_ID] = { sender, reply_to_me };
                return;
            }
        }
        else {
            switch (msg->State_Fail) {
                default:
                    break;
                case Conduits::Raw::FailState::failed_no_connexion:
                    reply.set_connexion_failure(true);
                    break;
                case Conduits::Raw::FailState::failed_no_response_expected:
                    reply.set_no_response_expected(true);
                    break;
                case Conduits::Raw::FailState::failed_timed_out:
                    reply.set_connexion_failure(true);
                    break;
                case Conduits::Raw::FailState::receiver_will_not_handle:
                    reply.set_receiver_will_not_handle(true);
                    break;
            };
        }
            // If the message has a response we add this
            // to the scratch object, too
        if (msg->Response) {
            reply.String = msg->Response->get_message_string();
            reply.String_Length = (uint16)strlen(reply.String);
            reply.add_segments_from_message(msg->Response);

                // If the response allows for a response we simply
                // add it to the pending messages; otherwise we can
                // simply release the response immediately
            if (Conduits::Raw::ResponseDesire::response_is_possible(msg->get_response_expectation())) {
                reply.Reply_To_Me_ID = this->internal_async_add_pending_message(sender, msg->Response);
            }
            else {
                msg->Response->set_OK();
                msg->Response->release();
            }
        }

		this->internal_async_send_message(sender, Conduits::Protocol::MessageScratch::try_stringify_message(reply));
    };

        // Set up rest of msg
    msg->Message_String.assign(intr.String, intr.String_Length);
    msg->add_message_segments_from_list(intr.Segments);

    if (intr.get_accepts_response()) {
        msg->Response_Desire = Conduits::Raw::ResponseDesire::allowed;
    }

    if (intr.get_is_response()) {
        this->Message_Nexus->setup_use_queue_for_release_event(msg);
        original_msg->set_response(msg);
        original_msg->release();
        return;
    }
    
    this->Message_Nexus->setup_use_queue_for_release_event(msg);
    this->Message_Nexus->send_on_conduit(intr.Recipient_ID, msg);
}

void Socketman::internal_conduit_to_socket_func(Conduits::Raw::ConduitRef ref, Conduits::Raw::IMessage *msg)
{
    Conduits::Protocol::MessageScratch reply;

    std::shared_lock<std::shared_mutex> lk(this->Mx_Conduit_To_Socket_Relay);

        // See if we actually can find our out socket
    auto it = this->Conduit_To_Socket_Map.find(ref);
    if (it == this->Conduit_To_Socket_Map.end()) {
        lk.unlock();

        msg->set_FAILED(Conduits::Raw::FailState::failed_no_connexion);
        msg->release();

        return;
    }

        // We send over the connexion to a conduit
        // (presumably) on the other side based on the id
    auto sender = it->second.Connexion;
    reply.Recipient_ID = it->second.Receiver_ID;

    lk.unlock();

        // If this message needs a reply add the message
        // to the correct pending list
    if (Conduits::Raw::ResponseDesire::response_is_possible(msg->get_response_expectation())) {
        reply.set_accepts_response(true);
        reply.Reply_To_Me_ID = this->internal_async_add_pending_message(sender, msg);
    }

    reply.String = msg->get_message_string();
    reply.String_Length = (uint16)strlen(reply.String);

    reply.add_segments_from_message(msg);
                
	this->internal_async_send_message(sender, Conduits::Protocol::MessageScratch::try_stringify_message(reply));
}

uint32 Socketman::internal_async_add_pending_message(WSConnexionPtr sender, IMessage * msg)
{
    auto id = this->get_unique_reply_id();

    std::lock_guard<std::shared_mutex> lk(this->Mx_Pending_Reply);
    this->Pending_Reply_Map[id] = SentAwayPendingReplyProp{ sender.lock(),  msg };

    return id;
}

    //  Connexion
    // --------------------
    
void Socketman::internal_sync_remove_connexion(WSConnexionPtrShared ptr)
{
    {   // Remove all conduit references to this socket
        std::lock_guard<std::shared_mutex> lk(this->Mx_Conduit_To_Socket_Relay);
        
        auto elem = this->Conduit_To_Socket_Map.begin();
        while (elem != this->Conduit_To_Socket_Map.end()) {
            if (elem->second.Connexion != ptr)
                continue;
                
            this->Message_Nexus->close_conduit(elem->first);
            elem = this->Conduit_To_Socket_Map.erase(elem);
        }
    }

        // Fail and remove pending messages related to connexion
    {   std::lock_guard<std::shared_mutex> lk(this->Mx_Pending_Reply);

        auto elem = this->Pending_Reply_Map.begin();
        while (elem != this->Pending_Reply_Map.end()) {
            if (elem->second.Connexion != ptr)
                continue;

            elem->second.Message_Waiting_For_Reply->set_FAILED(Conduits::Raw::FailState::failed_no_connexion);
            elem->second.Message_Waiting_For_Reply->release();

            elem = this->Pending_Reply_Map.erase(elem);
        }
    }
}