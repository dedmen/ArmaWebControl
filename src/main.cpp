#include <intercept.hpp>
#include <boost/beast.hpp>
#include "websocket.hpp"

int intercept::api_version() { //This is required for the plugin to work.
    return 1;
}

void intercept::register_interfaces() {
    
}
std::mutex frameLock;
std::shared_ptr<Server> serv;
void intercept::pre_start() {
    serv = std::make_shared<Server>();
    frameLock.lock();
}

void intercept::pre_init() {
    intercept::sqf::system_chat("The Intercept template plugin is running!");
}

void intercept::on_frame() {
    frameLock.unlock();
    frameLock.lock();
}