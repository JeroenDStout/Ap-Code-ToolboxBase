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
        std::map<std::string, std::string>        Whitelist;
        std::vector<std::unique_ptr<std::thread>> ListenThreads;

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