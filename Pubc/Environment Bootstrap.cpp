/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/JSON Merge.h"
#include "BlackRoot/Pubc/Files.h"

#include "ToolboxBase/Pubc/Base Messages.h"
#include "ToolboxBase/Pubc/Environment Bootstrap.h"

using namespace Toolbox::Util;

bool EnvironmentBootstrap::ExecuteFromFilePath(const BlackRoot::IO::FilePath path) {
    namespace IO   = BlackRoot::IO;
    namespace Mode = BlackRoot::IO::FileMode;
    using     cout = BlackRoot::Util::Cout;

    IO::BaseFileSource fm("");

    if (!fm.Exists(path)) {
        cout{} << "Bootstrap cannot find '" << path << "!" << std::endl;
        return false;
    }

    IO::BaseFileSource::FCont contents;
    Toolbox::Messaging::JSON   jsonCont;

    try {
        contents = fm.ReadFile(path, Mode::OpenInstr{}.Default().Share(Mode::Share::Read));
        jsonCont = Toolbox::Messaging::JSON::parse(contents);
    }
    catch (...) {
        cout{} << "Bootstrap error reading '" << path << "!" << std::endl;
        return false;
    }
    
    cout{} << "Bootstrap loading from '" << path << "!" << std::endl;
    
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