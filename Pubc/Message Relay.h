/*
 *
 *  © Stout Games 2019
 *
 */

#pragma once

#include "Toolbox/Interface Messages.h"

namespace Toolbox {
namespace Messaging {

    struct MessageRelayAsDir {
        template<typename C>
            using MessageClassFunc = void (C::*)(std::string, Toolbox::Messaging::IAsynchMessage*);

        struct RelayElement {
            using MessageFunc = std::function<void(std::string, Toolbox::Messaging::IAsynchMessage*)>;

            MessageFunc RelayMethod, DeliverMethod;

            RelayElement() {
            }

            RelayElement(MessageFunc relay, MessageFunc deliver)
            : RelayMethod(relay), DeliverMethod(deliver) {
            }
            
            void operator= (const RelayElement rh) {
                this->RelayMethod   = rh.RelayMethod;
                this->DeliverMethod = rh.DeliverMethod;
            }
        };

        std::map<std::string, RelayElement>  RelayMap;

        MessageRelayAsDir() {
        }
        
        template<typename C1, typename C2, typename F1 = MessageClassFunc<C1>, typename F2 = MessageClassFunc<C2>>
        void Emplace(std::string str, C1 *c1, F1 relayFunction, C2 *c2, F2 deliverFunction) {
            using std::placeholders::_1;
            using std::placeholders::_2;
            
            RelayElement elem(std::bind(relayFunction, c1, _1, _2), std::bind(deliverFunction, c2, _1, _2));
            this->RelayMap[str] = elem;

           // this->RelayMap.emplace(std::make_pair(str, elem));
        }

        template<typename C1,typename F1 = MessageClassFunc<C1>>
        void Emplace(std::string str, C1 *c1, F1 relayFunction, F1 deliverFunction) {
            this->Emplace(str, c1, relayFunction, deliverFunction);
        }

        void RelayMessageSafe(Toolbox::Messaging::IAsynchMessage*);
    };

}
}