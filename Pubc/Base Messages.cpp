/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "ToolboxBase\Pubc\Interface Environment.h"
#include "ToolboxBase\Pubc\Base Messages.h"

using namespace Toolbox::Messaging;

TB_MESSAGES_BEGIN_DEFINE(BaseMessageReceiver);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(BaseMessageReceiver, dir);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(BaseMessageReceiver, http);
TB_MESSAGES_END_DEFINE(BaseMessageReceiver);

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void BaseMessageReceiver::_dir(Toolbox::Messaging::IAsynchMessage * msg)
{
    std::vector<std::string> funcs;
    this->InternalMessageDir(funcs);

    msg->Response = funcs;
    msg->SetOK();
}

void BaseMessageReceiver::_http(Toolbox::Messaging::IAsynchMessage * msg)
{
	std::stringstream ss;

	ss << "<!doctype html>" << std::endl
		<< "<html>" << std::endl
		<< " <head>" << std::endl
		<< "  <title>Message Receiver</title>" << std::endl
		<< " </head>" << std::endl
		<< " <body>" << std::endl
		<< "  <h1>Base Message Receiver</h1>" << std::endl
		<< "  <p>This is a default Base Message Receiver page.</p>" << std::endl
		<< " </body>" << std::endl
		<< "</html>";

    msg->Response = { { "http", ss.str() } };
    msg->SetOK();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

SocketMessage::SocketMessage(std::weak_ptr<void> ptr, BlackRoot::Format::JSON json)
{
    this->SocketRef = ptr;
    this->Message = json;
}

void SocketMessage::SetOK()
{
    this->State = IAsynchMessage::StateType::OK;
}

void SocketMessage::SetFailed()
{
    this->State = IAsynchMessage::StateType::Failed;
}

void SocketMessage::Close()
{
    Toolbox::Core::GetEnvironment()->ReceiveDelegateMessageToSocket(this->SocketRef, this);
    delete this;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

AwaitMessage::AwaitMessage()
{
    this->Closed = false;
}

AwaitMessage::AwaitMessage(JSON json)
{
    this->Message = json;
    this->Closed = false;
}

void AwaitMessage::SetOK()
{
    this->State = IAsynchMessage::StateType::OK;
}

void AwaitMessage::SetFailed()
{
    this->State = IAsynchMessage::StateType::Failed;
}

void AwaitMessage::Close()
{
    this->Closed = true;

    std::unique_lock<std::mutex> lk(this->WaitMutex);
    this->ClosedCV.notify_all();
}

IAsynchMessage::StateType::Type AwaitMessage::Await()
{
    std::unique_lock<std::mutex> lk(this->WaitMutex);
    this->ClosedCV.wait(lk, std::bind(&AwaitMessage::IsClosed, this) );

    return this->State;
}

bool AwaitMessage::AwaitSuccess()
{
    IAsynchMessage::StateType::Type state = this->Await();
    return IAsynchMessage::StateType::IsOK(state);
}