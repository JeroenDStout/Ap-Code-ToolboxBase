/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <iostream>

#include "BlackRoot\Pubc\Threaded IO Stream.h"
#include "BlackRoot\Pubc\Sys Path.h"

#include "ToolboxBase/Pubc/Base Environment.h"
#include "ToolboxBase/Pubc/Base LogMan.h"
#include "ToolboxBase/Pubc/Base SocketMan.h"

using namespace Toolbox::Base;

TB_MESSAGES_BEGIN_DEFINE(BaseEnvironment);

TB_MESSAGES_ENUM_BEGIN_MEMBER_FUNCTIONS(BaseEnvironment);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(BaseEnvironment, close);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(BaseEnvironment, stats);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(BaseEnvironment, setRefDir);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(BaseEnvironment, createSocketMan);
TB_MESSAGES_ENUM_MEMBER_FUNCTION(BaseEnvironment, ping);
TB_MESSAGES_ENUM_END_MEMBER_FUNCTIONS(BaseEnvironment);

TB_MESSAGES_END_DEFINE(BaseEnvironment);

BaseEnvironment::BaseEnvironment()
{
    this->SocketMan = nullptr;
}

BaseEnvironment::~BaseEnvironment()
{
}

void BaseEnvironment::UnloadAll()
{
    if (this->SocketMan) {
        this->SocketMan->Deinitialise(nullptr);
    }
}

void BaseEnvironment::InternalSetupRelayMap()
{
    this->MessageRelay.Emplace("env",  this, &BaseEnvironment::InternalMessageRelayToNone, this, &BaseEnvironment::InternalMessageSendToEnv);
    this->MessageRelay.Emplace("sock", this, &BaseEnvironment::InternalMessageRelayToNone, this, &BaseEnvironment::InternalMessageSendToNone);
}

void BaseEnvironment::InternalMessageLoop()
{
    // Only one thread should be in here

    while (true) {
        std::unique_lock<std::mutex> lk(this->MessageWaitMutex);
        this->MessageCV.wait(lk, std::bind(&BaseEnvironment::InternalThreadsShouldBeAwake, this) );
        lk.unlock();
        
        Messaging::IAsynchMessage * message = nullptr;

        std::unique_lock<std::mutex> msml(this->MessageCueMutex);
        if (this->MessageCue.size() > 0) {
            message = this->MessageCue[0];
            this->MessageCue.erase(this->MessageCue.begin());
        }
        msml.unlock();

        if (message) {
            this->InternalHandleMessage(message);
        }

        if (this->CurrentState == StateType::ShouldClose)
            break;
    }
}

bool BaseEnvironment::InternalThreadsShouldBeAwake()
{
    return this->CurrentState != StateType::Running || this->MessageCue.size() > 0;
}

void BaseEnvironment::InternalHandleMessage(Toolbox::Messaging::IAsynchMessage * message)
{
    this->MessageRelay.RelayMessageSafe(message);
}

void BaseEnvironment::InternalMessageRelayToNone(std::string target, Toolbox::Messaging::IAsynchMessage *message)
{
    std::stringstream reply;
    reply << "The manager '" << target << "' cannot relay this.";

    message->SetFailed();
    message->Response = {"No relay", reply.str() };
    message->Close();
}

void BaseEnvironment::InternalMessageSendToNone(std::string target, Toolbox::Messaging::IAsynchMessage *message)
{
    std::stringstream reply;
    reply << "The manager '" << target << "' has not been loaded.";

    message->SetFailed();
    message->Response = {"Not loaded", reply.str() };
    message->Close();
}

void BaseEnvironment::InternalMessageSendToEnv(std::string, Toolbox::Messaging::IAsynchMessage *message)
{
    this->MessageReceiveImmediate(message);
}

void BaseEnvironment::ReceiveDelegateMessageAsync(Toolbox::Messaging::IAsynchMessage *message)
{
    std::unique_lock<std::mutex> lk(this->MessageCueMutex);
    this->MessageCue.push_back(message);
    lk.unlock();
    
    std::unique_lock<std::mutex> lk2(this->MessageWaitMutex);
    this->MessageCV.notify_one();
}

void BaseEnvironment::ReceiveDelegateMessageFromSocket(std::weak_ptr<void> ptr, std::string string)
{
    try {
        auto json = Toolbox::Messaging::JSON::parse(string);
        std::cout << ">>> " << json.dump() << std::endl;

        this->ReceiveDelegateMessageAsync(new Messaging::SocketMessage(ptr, json));
    }
    catch (...) {
        ;
    }
}

void BaseEnvironment::ReceiveDelegateMessageToSocket(std::weak_ptr<void> ptr, Toolbox::Messaging::IAsynchMessage * message)
{
    if (this->SocketMan) {
        std::stringstream ss;
        if (Toolbox::Messaging::IAsynchMessage::StateType::IsOK(message->State)) {
            ss << "{ \"Result\" : \"OK\", \"Reply\" : " << message->Response.dump() << "}";
        }
        else {
            ss << "{ \"Result\" : \"Failed\", \"Reply\" : " << message->Response.dump() << "}";
        }

        this->SocketMan->MessageSendImmediate(ptr, ss.str());
    }
    else {
        ;
    }
}

void BaseEnvironment::Run()
{
    using cout = BlackRoot::Util::Cout;

    this->InternalInitStats();

    this->InternalSetupRelayMap();

    cout{} << "[Environment] Running" << std::endl;

    this->InternalMessageLoop();

    cout{} << "[Environment] Closing" << std::endl;
}

void BaseEnvironment::Close()
{
    this->CurrentState = StateType::ShouldClose;
    
    std::unique_lock<std::mutex> lk(this->MessageWaitMutex);
    this->MessageCV.notify_all();
}

void BaseEnvironment::SetBootDir(FilePath path)
{
    this->EnvProps.BootDir = BlackRoot::System::MakePathCanonical(path);
}

void BaseEnvironment::SetRefDir(FilePath path)
{
    using cout = BlackRoot::Util::Cout;

    auto str = path.string();

    std::size_t pos;
    while (std::string::npos != (pos = str.find("{boot}"))) {
        str.replace(str.begin() + pos, str.begin() + pos + 6, this->EnvProps.BootDir.string());
    }

    this->EnvProps.ReferenceDir = BlackRoot::System::MakePathCanonical(str);
    
    cout{} << "Env: Reference dir is now" << std::endl << " " << this->EnvProps.ReferenceDir << std::endl;
}

BaseEnvironment::FilePath BaseEnvironment::GetRefDir()
{
    return this->EnvProps.ReferenceDir;
}

void BaseEnvironment::InternalInitStats()
{
    this->BaseStats.StartTime = std::chrono::high_resolution_clock::now();
}

void BaseEnvironment::InternalCompileStats(BlackRoot::Format::JSON & json)
{
    auto curTime = std::chrono::high_resolution_clock::now();

    json["Uptime"] = int(std::chrono::duration_cast<std::chrono::milliseconds>(curTime - this->BaseStats.StartTime).count());

    BlackRoot::Format::JSON & Managers = json["Managers"];
    Managers["SocketMan"] = this->SocketMan    ? "Loaded" : "Not Loaded";
    Managers["LogMan"]    = this->LogMan       ? "Loaded" : "Not Loaded";
}

void BaseEnvironment::_setRefDir(Toolbox::Messaging::IAsynchMessage * msg)
{
    std::string s = msg->Message.begin().value();

    this->SetRefDir(s);
    
    std::stringstream ss;
    ss << "Env: Reference directory is now '" << this->EnvProps.ReferenceDir << "'";

    msg->Response = { ss.str() };
    msg->SetOK();
}

void BaseEnvironment::_stats(Toolbox::Messaging::IAsynchMessage * msg)
{
    BlackRoot::Format::JSON ret;
    this->InternalCompileStats(ret);
    msg->Response = ret;

    msg->SetOK();
}

void BaseEnvironment::_ping(Toolbox::Messaging::IAsynchMessage * msg)
{
    std::cout << '\a' << "(pong)" << std::endl;

    msg->Response = { "Pong" };
    msg->SetOK();
}

void BaseEnvironment::_close(Toolbox::Messaging::IAsynchMessage * msg)
{
    msg->Response = { "Closing" };
    msg->SetOK();

    this->Close();
}

void BaseEnvironment::CreateSocketMan()
{
    Toolbox::Messaging::JSON param;

    this->SocketMan = this->AllocateSocketMan();
    this->SocketMan->Initialise( &param );
}

void BaseEnvironment::_createSocketMan(Toolbox::Messaging::IAsynchMessage * msg)
{
    if (this->SocketMan) {
        msg->Response = { "SocketMan already exists" };
        msg->SetFailed();
        return;
    }

    msg->Response = { "Creating SocketMan" };
    msg->SetOK();

    this->CreateSocketMan();
}

Toolbox::Core::ILogMan * BaseEnvironment::AllocateLogMan()
{
    return new Toolbox::Base::LogMan;
}

Toolbox::Core::ISocketMan * BaseEnvironment::AllocateSocketMan()
{
    return new Toolbox::Base::SocketMan;
}

bool BaseEnvironment::IsRunning()
{
    return this->CurrentState == StateType::Running;
}