#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <thread>


class Server {

public:
    Server();

    boost::asio::io_context ioc{ 1 };
    std::vector<std::thread> iothreads;
};
