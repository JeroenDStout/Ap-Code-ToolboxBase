/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/JSON Merge.h"
#include "BlackRoot/Pubc/Files Types.h"
#include "BlackRoot/Pubc/Sys Path.h"

#include "ToolboxBase/Pubc/Base Messages.h"
#include "ToolboxBase/Pubc/Environment Bootstrap.h"

using namespace Toolbox::Util;

void EnvironmentBootstrap::SetupEnvironment(Toolbox::Core::IEnvironment * env) {
    this->Environment = env;

    std::string bootDir = "";
    if (!this->BootPath.empty()) {
        bootDir = this->BootPath.string();
    }

    auto pos = bootDir.find_last_of(BlackRoot::System::DirSeperator);
    if (pos != std::string::npos) {
        bootDir = bootDir.substr(0, pos);
    }
    this->Environment->SetBootDir(bootDir);
    this->Environment->SetRefDir("{boot}");
}

bool EnvironmentBootstrap::ExecuteFromFilePath(const BlackRoot::IO::FilePath path) {
    namespace IO   = BlackRoot::IO;
    namespace Mode = BlackRoot::IO::FileMode;
    using     cout = BlackRoot::Util::Cout;

    IO::BaseFileSource fm("");

    if (!fm.Exists(path)) {
        cout{} << std::endl << "!! Bootstrap cannot find '" << path << "'!" << std::endl << std::endl;
        return false;
    }

    IO::BaseFileSource::FCont contents;
    Toolbox::Messaging::JSON   jsonCont;

    try {
        contents = fm.ReadFile(path, Mode::OpenInstr{}.Default().Share(Mode::Share::Read));
        jsonCont = Toolbox::Messaging::JSON::parse(contents);
    }
    catch (BlackRoot::Debug::Exception * ex) {
        cout{} << "!! Bootstrap error reading" << std::endl << " " << path << std::endl;
        cout{} << " " << ex->GetPrettyDescription() << std::endl;
        return false;
    }
    catch (...) {
        cout{} << "!! Bootstrap unknown error reading" << std::endl << " " << path << std::endl;
        return false;
    }
    
    cout{} << "Bootstrap loading from" << std::endl << " " << path << std::endl;
    
    BlackRoot::Util::JSONMerge merger(&fm, std::move(jsonCont));
    merger.MergeRecursively();

    return this->ExecuteFromJSON(merger.JSON);
}

bool EnvironmentBootstrap::ExecuteFromJSON(Toolbox::Messaging::JSON json)
{ 
    using cout = BlackRoot::Util::Cout;

    for (auto& el1 : json.items()) {
        if (0 == (el1.key().compare("serious"))) {
            for (auto& el2 : el1.value().items()) {
                cout{} <<  "> " << el2.value().dump() << std::endl;

                Toolbox::Messaging::AwaitMessage msg(el2.value());
                this->Environment->ReceiveDelegateMessageAsync(&msg);

                if (!msg.AwaitSuccess()) {
                    cout{} << "<## " << msg.Response.dump() << std::endl
                           << "    Error in serious startup!" << std::endl;
                    return false;
                }
            
                cout{} << "< " << msg.Response.dump() << std::endl;
            }
        }
        else {
            cout{} << "Bootstrap does not understand '" << el1.key() << "!" << std::endl;
            return false;
        }
    }

    return true;
}