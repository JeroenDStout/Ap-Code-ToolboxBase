/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <iostream>

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Version Reg.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/Sys Path.h"

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
CON_RMR_REGISTER_FUNC(BaseEnvironment, set_user_doc_dir);
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

    this->Message_Nexus->set_ad_hoc_message_handling([=](Conduits::Raw::ConduitRef, Conduits::Raw::IRelayMessage * msg){
        this->rmr_handle_message_immediate_and_release(msg);
    });

    this->Simple_Relay.Call_Map["env"] = [=](Conduits::Raw::IRelayMessage * msg) {
        return this->rmr_handle_message_immediate(msg);
    };

    this->Simple_Relay.Call_Map["web"] = [=](Conduits::Raw::IRelayMessage * msg) {
        BlackRoot::IO::BaseFileSource s;
        Conduits::Util::HttpFileServer server;
        server.Inner_Source = &s;
        server.handle(this->get_ref_dir() / "Web", msg->get_adapting_path(), msg);
        msg->set_OK();
        return true;
    };
    this->Simple_Relay.Call_Map["favicon.ico"] = [=](Conduits::Raw::IRelayMessage * msg) {
        BlackRoot::IO::BaseFileSource s;
        Conduits::Util::HttpFileServer server;
        server.Inner_Source = &s;
        server.handle(this->get_ref_dir() / "Web" / this->internal_get_favicon_name(), "", msg);
        msg->set_OK();
        return true;
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

    cout{} << "~*~*~ Environment Running ~*~*~" << std::endl;
    
    this->Message_Nexus->start_accepting_threads();
    while (!this->internal_get_thread_should_interrupt()) {
        this->Message_Nexus->await_message_and_handle();
    }

    cout{} << "~*~*~ Environment Closing ~*~*~" << std::endl;
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
        [this](Conduits::Raw::ConduitRef, Conduits::Raw::IRelayMessage * msg)
    {
        this->rmr_handle_message_immediate_and_release(msg);
    };

    auto id = this->Message_Nexus->manual_open_conduit_to(this->Socketman->get_en_passant_nexus(), info_func, message_message);
    this->Socketman->connect_en_passant_conduit(id.second);

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

void BaseEnvironment::set_user_doc_dir(FilePath path)
{
    using cout = BlackRoot::Util::Cout;

    this->Env_Props.User_Doc_Dir = this->expand_dir(path);
    cout{} << "Env: User documents dir is now" << std::endl << " " << this->Env_Props.User_Doc_Dir << std::endl;

        // We ensure the path, just to be sure, and by design
        // we should have permission to create directories
        // in this specific path
    fs::create_directories(this->Env_Props.User_Doc_Dir);
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

        // Replace my doc path
    while (std::string::npos != (pos = str.find("{os-my-doc}"))) {
        str.replace(str.begin() + pos, str.begin() + pos + 11, BlackRoot::System::GetUserDocumentsPath().string());
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

BaseEnvironment::FilePath BaseEnvironment::get_user_doc_dir()
{
    return this->Env_Props.User_Doc_Dir;
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

void BaseEnvironment::async_receive_message(Conduits::Raw::IRelayMessage * msg)
{
    this->Message_Nexus->async_add_ad_hoc_message(msg);
}

void BaseEnvironment::_create_logman(Conduits::Raw::IRelayMessage * msg) noexcept
{
    this->savvy_try_wrap_read_json(msg, 0, [&](JSON json) {
        DbAssertMsgFatal(!this->Logman, "Logman already exists");
        this->create_logman(json);
        msg->set_OK();
    });
}

void BaseEnvironment::_create_socketman(Conduits::Raw::IRelayMessage * msg) noexcept
{
    this->savvy_try_wrap_read_json(msg, 0, [&](JSON json) {
        DbAssertMsgFatal(!this->Socketman, "Socketman already exists");
        this->create_socketman(json);
        msg->set_OK();
    });
}

void BaseEnvironment::_stats(Conduits::Raw::IRelayMessage * msg) noexcept
{
    this->savvy_try_wrap_write_json(msg, 0, [&] {
        JSON ret;
        this->internal_compile_stats(ret);
        msg->set_OK();
        return ret;
    });
}

void BaseEnvironment::_code_credits(Conduits::Raw::IRelayMessage * msg) noexcept
{
    this->savvy_try_wrap_write_json(msg, 0, [&] {
        JSON ret = { BlackRoot::Repo::VersionRegistry::GetBootString() };
        msg->set_OK();
        return ret;
    });
}

void BaseEnvironment::_set_ref_dir(Conduits::Raw::IRelayMessage * msg) noexcept
{
    using cout = BlackRoot::Util::Cout;

    this->savvy_try_wrap_read_json(msg, 0, [&](JSON json) {
        auto dir = Toolbox::Core::Get_Environment()->get_ref_dir();
        if (json.is_object()) {
            json = json["path"];
        }
        DbAssertMsgFatal(json.is_string(), "Malformed JSON: cannot get path");
        
        this->set_ref_dir(json.get<JSON::string_t>());

        std::string rt = "Ref dir has been set to ";
        rt += this->get_ref_dir().string();

        msg->set_response_string_with_copy(rt.c_str());
        msg->set_OK();
    });
}

void BaseEnvironment::_set_user_doc_dir(Conduits::Raw::IRelayMessage * msg) noexcept
{
    using cout = BlackRoot::Util::Cout;

    this->savvy_try_wrap_read_json(msg, 0, [&](JSON json) {
        auto dir = Toolbox::Core::Get_Environment()->get_ref_dir();
        if (json.is_object()) {
            json = json["path"];
        }
        DbAssertMsgFatal(json.is_string(), "Malformed JSON: cannot get path");
        
        this->set_user_doc_dir(json.get<JSON::string_t>());

        std::string rt = "User documents dir has been set to ";
        rt += this->get_user_doc_dir().string();

        msg->set_response_string_with_copy(rt.c_str());
        msg->set_OK();
    });
}

void BaseEnvironment::_ping(Conduits::Raw::IRelayMessage * msg) noexcept
{
    using cout = BlackRoot::Util::Cout;
    cout{} << '\a' << "(pong)" << std::endl;

    this->savvy_try_wrap_write_json(msg, 0, [&] {
        JSON ret = { "pong" };
        msg->set_OK();
        return ret;
    });
}

void BaseEnvironment::_close(Conduits::Raw::IRelayMessage * msg) noexcept
{
    using cout = BlackRoot::Util::Cout;
    
    this->async_close();
    msg->set_OK();
}