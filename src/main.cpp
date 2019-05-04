#include <intercept.hpp>
#include <boost/beast.hpp>
#include "websocket.hpp"

int intercept::api_version() { //This is required for the plugin to work.
    return INTERCEPT_SDK_API_VERSION;
}

void intercept::register_interfaces() {
    
}

std::shared_ptr<Server> serv;
void intercept::pre_start() {
    serv = std::make_shared<Server>();
}

void intercept::pre_init() {
    intercept::sqf::system_chat("The Intercept template plugin is running!");
}
extern std::set<std::shared_ptr<websocket_session>> wsSessions;
void intercept::on_frame() {
    for (auto& it : wsSessions) {
        it->processTasks();
    }
}