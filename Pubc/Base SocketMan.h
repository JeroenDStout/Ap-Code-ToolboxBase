/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <thread>
#include "ToolboxBase/Pubc/Interface SocketMan.h"

namespace Toolbox {
namespace Base {

	class SocketMan : public Core::ISocketMan {
    public:


	protected:
        class Server;
        class Connection;

        friend Server;

        typedef std::weak_ptr<void> ConnectionPtr;

        Server  *InternalServer;

        std::vector<std::pair<std::string, ConnectionPtr>> 
                                                         OpenConnections;
        std::vector<std::pair<std::string, std::regex>>  Whitelist;
        std::vector<std::unique_ptr<std::thread>>        ListenThreads;

        virtual void InternalServerListen();
        virtual void InternalServerTalk();

        virtual bool InternalValidateConnection(std::string ip, std::string port, std::string & outName);
        virtual bool InternalReceiveAdHocCommand(ConnectionPtr, std::string);
    public:
        ~SocketMan() override { ; }

        void    Initialise(BlackRoot::Format::JSON * param) override;
        void    Deinitialise(BlackRoot::Format::JSON * param) override;

        void    MessageSendImmediate(std::weak_ptr<void>, std::string) override;

        virtual int GetDefaultPort() { return 3001; }
	};

}
}