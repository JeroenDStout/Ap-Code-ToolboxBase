/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// TODO: Replace the complicated recursive calling child types with a
// single map which calls proper functions

#pragma once

#include <map>
#include <string>

#include "BlackRoot/Pubc/Stringstream.h"
#include "BlackRoot\Pubc\JSON.h"

namespace Toolbox {
namespace Messaging {

    typedef BlackRoot::Format::JSON JSON;

    class IAsynchMessage {
    public:
        struct StateType {
            typedef unsigned char Type;

            enum : Type {
                Queued  = 0x1 << 0,
                OK      = 0x1 << 1,
                Failed  = 0x1 << 2,
                Aborted = 0x1 << 7
            };

            static inline bool IsOK(Type t) {
                return (0 != (t & OK)) && (0 == (t & (Aborted | Failed)));
            }
        };

    public:
        StateType::Type     State;

    public:
        JSON Message;
        JSON Response;
        
        virtual ~IAsynchMessage() { ; }
        
        virtual void SetOK() = 0;
        virtual void SetFailed() = 0;
        virtual void Close() = 0;
    };

    class IMessageReceiver {
    protected:
        inline virtual bool InternalMessageCallFunction(std::string s, Toolbox::Messaging::IAsynchMessage *m) {
            return false;
        }
        inline virtual bool InternalMessageDir(std::vector<std::string> & v) {
            return false;
        }

    public:
        virtual ~IMessageReceiver() = 0 { ; }

        void MessageReceiveImmediate(IAsynchMessage *);
    };

    inline void IMessageReceiver::MessageReceiveImmediate(IAsynchMessage *message)
    {
        std::string funcName = "!error!";

        if (message->Message.is_string()) {
            funcName = message->Message.get<std::string>();
        }
        else if (message->Message.is_array()) {
            funcName = message->Message[0].get<std::string>();
        }
        else if (message->Message.is_object()) {
            funcName = message->Message.begin().key();
        }

        if (this->InternalMessageCallFunction(funcName, message))
            return;

        std::stringstream reply;
        reply << "No such function: '" << funcName << "'";

        message->SetFailed();
        message->Response = { "Failed", "NoFunc", reply.str() };
        message->Close();
    }

    namespace Macros {
        
        template<typename C>
        struct FunctionMap {
            typedef void (C::*MFP)(Toolbox::Messaging::IAsynchMessage *);

            std::map<std::string, MFP> functionMap;
            inline bool TryCall(C * c, std::string s, Toolbox::Messaging::IAsynchMessage *m) {
                std::map<std::string, MFP>::iterator it;
                it = functionMap.find(s);
                if (it != functionMap.end()) {
                    (c->*(it->second))(m);
                    return true;
                }
                return false;
            }
            FunctionMap() { ; }
        };

        template<typename C>
        struct FunctionConnector {
            typedef void (C::*MFP)(Toolbox::Messaging::IAsynchMessage *);

            FunctionConnector(FunctionMap<C> * map, char * name, MFP f) {
                map->functionMap.insert( { name, f } );
            }
        };

    }

#define TB_MESSAGES_CONCAT(x, y) x ## y
#define TB_MESSAGES_CONCAT2(x, y) TB_MESSAGES_CONCAT(x, y)

#define TB_MESSAGES_DECLARE_RECEIVER(x, baseclass) \
    typedef x         MessengerCurClass; \
    typedef baseclass MessengerBaseClass; \
    public: \
    struct __MessageReceiverProps##x { \
        Toolbox::Messaging::Macros::FunctionMap<x> FunctionMap; \
    };\
    static __MessageReceiverProps##x _MessageReceiverProps##x; \
    \
    inline bool InternalMessageCallFunction(std::string s, Toolbox::Messaging::IAsynchMessage *m) override { \
        if (_MessageReceiverProps##x.FunctionMap.TryCall(this, s, m)) { \
            m->Close(); \
            return true; \
        } \
        return this->MessengerBaseClass::InternalMessageCallFunction(s, m); \
    } \
    \
    inline bool InternalMessageDir(std::vector<std::string> & v) override { \
        this->MessengerBaseClass::InternalMessageDir(v); \
        for(auto const & s : _MessageReceiverProps##x.FunctionMap.functionMap) { \
            v.push_back(s.first); \
        } \
        return true; \
    }

#define TB_MESSAGES_DECLARE_MEMBER_FUNCTION(func) \
    public: void _##func(Toolbox::Messaging::IAsynchMessage*);

#define TB_MESSAGES_BEGIN_DEFINE(x) \
    x::__MessageReceiverProps##x x::_MessageReceiverProps##x = { };

#define TB_MESSAGES_END_DEFINE(x)
    
#define TB_MESSAGES_ENUM_BEGIN_MEMBER_FUNCTIONS(x)
    
#define TB_MESSAGES_ENUM_MEMBER_FUNCTION(x, func) \
    static Toolbox::Messaging::Macros::FunctionConnector<x> FunctionConnector##x##func(&(x::_MessageReceiverProps##x.FunctionMap), #func, &x::_##func);

#define TB_MESSAGES_ENUM_END_MEMBER_FUNCTIONS(x)

}
}