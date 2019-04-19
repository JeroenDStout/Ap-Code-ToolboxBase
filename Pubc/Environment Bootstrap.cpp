/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "BlackRoot/Pubc/Assert.h"
#include "BlackRoot/Pubc/Threaded IO Stream.h"
#include "BlackRoot/Pubc/JSON Merge.h"
#include "BlackRoot/Pubc/Files Types.h"
#include "BlackRoot/Pubc/Sys Path.h"

#include "Conduits/Pubc/Interface Raw Message.h"
#include "Conduits/Pubc/Await Message.h"
#include "Conduits/Pubc/Path Tools.h"

#include "ToolboxBase/Pubc/Environment Bootstrap.h"

using namespace Toolbox::Util;

void EnvironmentBootstrap::setup_environment(Env * env) {
    this->Environment = env;

        // Our boot dir is the boot file directory
    std::string bootDir = "";
    if (!this->Boot_Path.empty()) {
        bootDir = this->Boot_Path.parent_path().string();
    }

        // Set up the environment; {boot} is a replacement
        // for the boot dir
    this->Environment->set_boot_dir(bootDir);
    this->Environment->set_ref_dir("{boot}");
}

bool EnvironmentBootstrap::execute_from_file_path(const Path path) {
    namespace IO   = BlackRoot::IO;
    namespace Mode = BlackRoot::IO::FileMode;
    using     cout = BlackRoot::Util::Cout;

    IO::BaseFileSource fm;

        // Check if file actually exists
    if (!fm.Exists(path)) {
        cout{} << std::endl << "!! Bootstrap cannot find '" << path << "'!" << std::endl << std::endl;
        return false;
    }

    IO::BaseFileSource::FCont contents;
    JSON jsonCont;

        // Try to read and parse the file
    try {
        contents = fm.ReadFile(path, Mode::OpenInstr{}.Default().Share(Mode::Share::Read));
        jsonCont = JSON::parse(contents);
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
    
        // Merge the JSON and execute
    BlackRoot::Util::JSONMerge merger(&fm, &jsonCont);
    merger.MergeRecursively();
    return this->execute_from_json(jsonCont);
}

bool EnvironmentBootstrap::execute_from_json(const JSON json)
{ 
    using cout = BlackRoot::Util::Cout;

    for (const auto & el1 : json.items())
    {
            // 'Serious' keys break the boot on startup;
            // in reality most of them will be serious
        if (0 == (el1.key().compare("serious"))) {
            DbAssertFatal(el1.value().is_array());

                // Every key is the path, its object is the values
                // i.e., { "path/to/object/action" : { "param" : "value" } }
            for (const auto & el2 : el1.value()) {
                DbAssertFatal(el2.is_object());

                cout{} <<  "> " << el2.dump() << std::endl;

                    // Create an await message holding our stringified JSON
                Conduits::BaseAwaitMessage await;
                await.Path = Conduits::Util::Sanitise_Path(el2.begin().key());
                await.Message_Segments[0] = el2.begin().value().dump();

                    // Prepare, send, and await; env is running this asynch
                await.sender_prepare_for_send();
                this->Environment->async_receive_message(&await);
                await.sender_await();

                    // If we fail, print a reason and abort startup
                if (await.Message_State != Conduits::MessageState::ok) {
                    cout{} << "<## " << await.Response_String << std::endl
                           << "    Error in serious startup!" << std::endl;
                    return false;
                }
            
                    // If there is a meaningful response, print it
                if (await.Response_String.length() > 0) {
                    cout{} << "< " << await.Response_String << std::endl;
                }
            }
        }
        else {
            cout{} << "Bootstrap does not understand '" << el1.key() << "!" << std::endl;
            return false;
        }
    }

    return true;
}