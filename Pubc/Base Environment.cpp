/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <iostream>

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Version Reg.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Sys Path.h"
#include "BlackRoot/Pubc/Sys Sound.h"
#include "BlackRoot/Pubc/Sys Alert.h"

#include "Conduits/Pubc/File Server.h"

#include "ToolboxBase/Pubc/Base Environment.h"
#include "ToolboxBase/Pubc/Base Logman.h"
#include "ToolboxBase/Pubc/Base Socketman.h"

using namespace Toolbox::Base;
namespace fs = std::experimental::filesystem;

    //  Relay message receiver
    // --------------------

CON_RMR_DEFINE_CLASS(BaseEnvironment);

CON_RMR_REGISTER_FUNC(BaseEnvironment, set_ref_dir);
CON_RMR_REGISTER_FUNC(BaseEnvironment, set_user_dir);
CON_RMR_REGISTER_FUNC(BaseEnvironment, stats);
CON_RMR_REGISTER_FUNC(BaseEnvironment, code_credits);
CON_RMR_REGISTER_FUNC(BaseEnvironment, create_logman);
CON_RMR_REGISTER_FUNC(BaseEnvironment, create_socketman);
CON_RMR_REGISTER_FUNC(BaseEnvironment, close);
CON_RMR_REGISTER_FUNC(BaseEnvironment, ping);

    //  Setup
    // --------------------

BaseEnvironment::BaseEnvironment()
{
    this->Logman      = nullptr;
    this->Socketman   = nullptr;

    this->Message_Nexus->set_ad_hoc_message_handling(
        std::bind(&BaseEnvironment::rmr_handle_message_immediate_and_release, this, std::placeholders::_1)
    );

        // Add base relays

    this->Simple_Relay.Call_Map["env"] =
        std::bind(&BaseEnvironment::rmr_handle_message_immediate_and_release, this, std::placeholders::_1);

    this->Simple_Relay.Call_Map["web"] = [=](Conduits::Raw::IMessage * msg) {
		this->internal_handle_web_request(msg->get_adapting_string(), msg);
        msg->release();
    };
    this->Simple_Relay.Call_Map["favicon.ico"] = [=](Conduits::Raw::IMessage * msg) {
		this->internal_handle_web_request(this->internal_get_favicon_name(), msg);
        msg->release();
    };
}

BaseEnvironment::~BaseEnvironment()
{
}

    //  Control
    // --------------------

void BaseEnvironment::run_with_current_thread()
{
    using cout = BlackRoot::Util::Cout;

    this->internal_init_stats();

    cout{} << std::endl << "~*~*~ Environment Running ~*~*~" << std::endl << std::endl;
    
    this->Message_Nexus->start_accepting_messages();
    this->Message_Nexus->start_accepting_threads();

    while (!this->internal_get_thread_should_interrupt()) {
        this->Message_Nexus->await_message_and_handle();
    }

    this->Message_Nexus->stop_gracefully();

    cout{} << std::endl << "~*~*~ Environment Closing ~*~*~" << std::endl << std::endl;
}

void BaseEnvironment::async_close()
{
    this->Current_State = StateType::should_close;
    this->Message_Nexus->stop_accepting_threads_and_return();
}

void BaseEnvironment::internal_init_stats()
{
    this->Base_Stats.Start_Time     = std::chrono::high_resolution_clock::now();
}

void BaseEnvironment::internal_unload_all()
{
    if (this->Logman) {
        this->Logman->deinitialise({});
    }
    if (this->Socketman) {
        this->Socketman->deinitialise_and_wait({});
    }
    
    this->Simple_Relay.Call_Map.erase("log");
    this->Simple_Relay.Call_Map.erase("sock");
}

void BaseEnvironment::create_logman(const JSON)
{
    using namespace std::placeholders;

    DbAssertFatal(nullptr == this->Logman);

    this->Logman = this->internal_allocate_logman();    
    this->Logman->initialise({});

    this->Simple_Relay.Call_Map["log"] = std::bind(&Core::ILogman::rmr_handle_message_immediate, this->Logman, _1);
}

void BaseEnvironment::create_socketman(const JSON json)
{
    using namespace std::placeholders;

    DbAssertFatal(nullptr == this->Socketman);

    JSON arg;

    auto it = json.find("port");
    if (it != json.end()) {
        arg["port"] = it.value().get<uint16_t>();
    }

    this->Socketman = this->internal_allocate_socketman();
    this->Socketman->initialise(arg);

        // We assume that socketman should route its en passant
        // messages to us, so set up the conduit
    Conduits::BaseNexus::InfoFunc info_func =
        [this](Conduits::Raw::ConduitRef, const Conduits::ConduitUpdateInfo)
    {
    };
    Conduits::BaseNexus::MessageFunc message_message =
        [this](Conduits::Raw::ConduitRef, Conduits::Raw::IMessage * msg)
    {
        this->rmr_handle_message_immediate_and_release(msg);
    };

        // Open the conduit for us and socketman
    auto id = this->Message_Nexus->manual_open_conduit_to(this->Socketman->get_en_passant_nexus(), info_func, message_message);
    this->Socketman->connect_en_passant_conduit(id.second);

        // Add socketman reference
    this->Simple_Relay.Call_Map["sock"] = std::bind(&Core::ISocketman::rmr_handle_message_immediate, this->Socketman, _1);
}

    //  Settings
    // --------------------

void BaseEnvironment::set_boot_dir(FilePath path)
{
    this->Env_Props.Boot_Dir = fs::canonical(path);
}

void BaseEnvironment::set_ref_dir(FilePath path)
{
    using cout = BlackRoot::Util::Cout;

    this->Env_Props.Reference_Dir = this->expand_dir(path);
    cout{} << "Env: Reference dir is now" << std::endl << " " << this->Env_Props.Reference_Dir << std::endl;
}

void BaseEnvironment::set_user_dir(FilePath path)
{
    using cout = BlackRoot::Util::Cout;

    this->Env_Props.User_Dir = this->expand_dir(path);
    cout{} << "Env: User documents dir is now" << std::endl << " " << this->Env_Props.User_Dir << std::endl;

        // We ensure the path, just to be sure, and by design
        // we should have permission to create directories
        // in this specific path
    fs::create_directories(this->Env_Props.User_Dir);
}

    //  Typed
    // --------------------

Toolbox::Core::ILogman * BaseEnvironment::internal_allocate_logman()
{
    return new Toolbox::Base::Logman;
}

Toolbox::Core::ISocketman * BaseEnvironment::internal_allocate_socketman()
{
    return new Toolbox::Base::Socketman;
}

    //  Util
    // --------------------

bool BaseEnvironment::internal_get_thread_should_interrupt()
{
    return this->Current_State != StateType::running;
}

void BaseEnvironment::internal_compile_stats(JSON & json)
{
    BlackRoot::Format::JSON & Managers = json["Managers"];
    Managers["logman"]    = this->Logman    ? "Loaded" : "Not Loaded";
    Managers["socketman"] = this->Socketman ? "Loaded" : "Not Loaded";
    
    auto cur_time = std::chrono::high_resolution_clock::now();
    json["uptime"] = int(std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - this->Base_Stats.Start_Time).count());
}

BaseEnvironment::FilePath BaseEnvironment::expand_dir(FilePath path)
{
    using cout = BlackRoot::Util::Cout;

        // TODO this is not thread safe with setting changes
        // TODO use more elegant solution (map) if this gets more complex

    auto str = path.string();
    std::size_t pos;

        // Replace boot path
    while (std::string::npos != (pos = str.find("{boot}"))) {
        str.replace(str.begin() + pos, str.begin() + pos + 6, this->Env_Props.Boot_Dir.string());
    }

        // Replace reference path
    while (std::string::npos != (pos = str.find("{ref}"))) {
        str.replace(str.begin() + pos, str.begin() + pos + 5, this->Env_Props.Reference_Dir.string());
    }

        // Replace roaming path
    while (std::string::npos != (pos = str.find("{os-roaming}"))) {
        str.replace(str.begin() + pos, str.begin() + pos + 12, BlackRoot::System::GetRoamingPath().string());
    }

        // Return expanded our dir
    path = fs::canonical(str);
    return path;
}

bool BaseEnvironment::get_is_running()
{
    return this->Current_State == StateType::running;
}

BaseEnvironment::FilePath BaseEnvironment::get_ref_dir()
{
    return this->Env_Props.Reference_Dir;
}

BaseEnvironment::FilePath BaseEnvironment::get_user_dir()
{
    return this->Env_Props.User_Dir;
}

void BaseEnvironment::internal_handle_web_request(std::string path, Conduits::Raw::IMessage * msg)
{
        // Quick and dirty way of having the conduits
        // server handle these rqeuests
        // TODO: less dirty way
    BlackRoot::IO::BaseFileSource s;
    Conduits::Util::HttpFileServer server;
    server.Inner_Source = &s;
    server.handle(this->get_ref_dir() / "Web", path.c_str(), msg);

    msg->set_OK();
}

    //  HTML
    // --------------------

void BaseEnvironment::savvy_handle_http(const JSON httpRequest, JSON & httpReply, std::string & outBody)
{
    std::stringstream ss;
    
    const auto & class_name = this->internal_get_rmr_class_name();
    auto registry  = BlackRoot::Repo::VersionRegistry::GetRegistry();
    auto main_proj = registry->GetMainProjectVersion();

	ss << "<!doctype html>" << std::endl
		<< "<html>" << std::endl
		<< " <head>" << std::endl
		<< "  <title>" << main_proj.Name << " - Base Environment</title>" << std::endl
		<< " </head>" << std::endl
		<< " <body>" << std::endl
		<< "  <h1>" << main_proj.Name << "</h1>" << std::endl
		<< "  <p>" << main_proj.Version << "<br/>" << main_proj.BuildTool << "</p>" << std::endl
		<< "  <h1>" << class_name << " (base relay)</h1>" << std::endl
		<< "  <p>" << this->html_create_action_relay_string() << "</p>" << std::endl;

        // Print contribution and version strings
    std::string version;
    version = registry->GetVersionString();

    std::string from = "\n";
    std::string to = "<br/>";
    size_t start_pos = 0;
    while((start_pos = version.find(from, start_pos)) != std::string::npos) {
        version.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    
    std::string contribution;
    contribution = registry->GetFullContributionString();
    start_pos = 0;
    while((start_pos = contribution.find(from, start_pos)) != std::string::npos) {
        contribution.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }

	ss << "  <h1>About</h1>" << std::endl
	   << "  <p>" << contribution << "</p>" << std::endl
	   << "  <p>" << version << "</p>" << std::endl;

    ss  << " </body>" << std::endl
		<< "</html>";

    outBody = ss.str();
}

    //  Messages
    // --------------------

void BaseEnvironment::async_receive_message(Conduits::Raw::IMessage * msg)
{
    this->Message_Nexus->async_add_ad_hoc_message(msg);
}

void BaseEnvironment::_create_logman(Conduits::Raw::IMessage * msg) noexcept
{
    this->savvy_try_wrap_read_json(msg, "", [&](JSON json) {
        DbAssertMsgFatal(!this->Logman, "Logman already exists");
        this->create_logman(json);
        msg->set_OK();
    });
}

void BaseEnvironment::_create_socketman(Conduits::Raw::IMessage * msg) noexcept
{
    this->savvy_try_wrap_read_json(msg, "", [&](JSON json) {
        DbAssertMsgFatal(!this->Socketman, "Socketman already exists");
        this->create_socketman(json);
        msg->set_OK();
    });
}

void BaseEnvironment::_stats(Conduits::Raw::IMessage * msg) noexcept
{
    this->savvy_try_wrap(msg, [&] {
        JSON ret;
        this->internal_compile_stats(ret);
        
            // Dump stats json in nameless segment
        std::unique_ptr<Conduits::DisposableMessage> reply(new Conduits::DisposableMessage());
        reply->Segment_Map[""] = ret.dump();
        reply->sender_prepare_for_send();

        msg->set_response(reply.release());
        msg->set_OK();
    });
}

void BaseEnvironment::_code_credits(Conduits::Raw::IMessage * msg) noexcept
{
    this->savvy_try_wrap(msg, [&] {
            // Set message to boot string
        std::unique_ptr<Conduits::DisposableMessage> reply(new Conduits::DisposableMessage());
        reply->Message_String = BlackRoot::Repo::VersionRegistry::GetBootString();
        reply->sender_prepare_for_send();
        
        msg->set_response(reply.release());
        msg->set_OK();
    });
}

void BaseEnvironment::_set_ref_dir(Conduits::Raw::IMessage * msg) noexcept
{
    using cout = BlackRoot::Util::Cout;

    this->savvy_try_wrap_read_json(msg, 0, [&](JSON json) {
        auto dir = Toolbox::Core::Get_Environment()->get_ref_dir();
        if (json.is_object()) {
            json = json["path"];
        }
        DbAssertMsgFatal(json.is_string(), "Malformed JSON: cannot get path");
        
            // Internally update reference dir
        this->set_ref_dir(json.get<JSON::string_t>());

            // Confirm, if needed, the effective reference dir
        if (Conduits::Raw::ResponseDesire::response_is_possible(msg->get_response_expectation())) {
            std::string rt = "Ref dir has been set to ";
            rt += this->get_ref_dir().string();

            std::unique_ptr<Conduits::DisposableMessage> reply(new Conduits::DisposableMessage());
            reply->Message_String = rt;
            reply->sender_prepare_for_send();
            msg->set_response(reply.release());
        }

        msg->set_OK();
    });
}

void BaseEnvironment::_set_user_dir(Conduits::Raw::IMessage * msg) noexcept
{
    using cout = BlackRoot::Util::Cout;

    this->savvy_try_wrap_read_json(msg, 0, [&](JSON json) {
        auto dir = Toolbox::Core::Get_Environment()->get_ref_dir();
        if (json.is_object()) {
            json = json["path"];
        }
        DbAssertMsgFatal(json.is_string(), "Malformed JSON: cannot get path");
        
            // Internally update user dir
        this->set_user_dir(json.get<JSON::string_t>());

            // Confirm, if needed, the effective user dir
        if (Conduits::Raw::ResponseDesire::response_is_possible(msg->get_response_expectation())) {
            std::string rt = "User dir has been set to ";
            rt += this->get_user_dir().string();

            std::unique_ptr<Conduits::DisposableMessage> reply(new Conduits::DisposableMessage());
            reply->Message_String = rt;
            reply->sender_prepare_for_send();
            msg->set_response(reply.release());
        }

        msg->set_OK();
    });
}

void BaseEnvironment::_ping(Conduits::Raw::IMessage * msg) noexcept
{
    this->savvy_try_wrap(msg, [&] {
        using cout = BlackRoot::Util::Cout;
        cout{} << '\a' << "(pong)" << std::endl;
    
        BlackRoot::System::PlayAdHocSound(this->get_ref_dir() / "Data/ping.wav");
        BlackRoot::System::FlashCurrentWindow();

        std::unique_ptr<Conduits::DisposableMessage> reply(new Conduits::DisposableMessage());
        reply->Message_String = "(pong)";
        reply->sender_prepare_for_send();
        msg->set_response(reply.release());
        msg->set_OK();
    });
}

void BaseEnvironment::_close(Conduits::Raw::IMessage * msg) noexcept
{
    using cout = BlackRoot::Util::Cout;
    
    this->async_close();
    msg->set_OK();
}