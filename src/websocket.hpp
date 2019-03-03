#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <thread>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/make_unique.hpp>
#include <boost/config.hpp>
#include "json.hpp"
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <intercept.hpp>
#include <filesystem>

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;            // from <boost/beast/http.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>



class Task {
public:
    json message;
    bool text;
};


class http_session;


// Report a failure
void fail(boost::system::error_code ec, char const* what);


//------------------------------------------------------------------------------

// Echoes back all received WebSocket messages
class websocket_session : public std::enable_shared_from_this<websocket_session> {
    websocket::stream<tcp::socket> ws_;
    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;
    boost::asio::steady_timer timer_;
    boost::beast::multi_buffer buffer_;
    char ping_state_ = 0;

    std::vector<Task> todoTasks;
    std::vector<Task> completedTasks;
    std::mutex taskMutex;
public:
    // Take ownership of the socket
    explicit websocket_session(tcp::socket socket);

    // Start the asynchronous operation
    template<class Body, class Allocator>
    void do_accept(http::request<Body, http::basic_fields<Allocator>> req);

    void on_accept(boost::system::error_code ec);

    // Called when the timer expires.
    void on_timer(boost::system::error_code ec);

    // Called to indicate activity from the remote peer
    void activity();

    // Called after a ping is sent.
    void on_ping(boost::system::error_code ec);

    void on_control_callback(
        websocket::frame_type kind,
        boost::beast::string_view payload);

    void do_read();

    json processTask(const json& task);

    Task doTask(const Task& input) {
        auto answer = processTask(input.message);
        return Task{ answer, input.text };
    }

    void processTasks();

    void on_read(boost::system::error_code ec, std::size_t bytes_transferred);

    void finishTasks();

    void on_write(
        boost::system::error_code ec,
        std::size_t bytes_transferred);
};

template <class Body, class Allocator>
void websocket_session::do_accept(http::request<Body, http::basic_fields<Allocator>> req) {
    // Set the control callback. This will be called
    // on every incoming ping, pong, and close frame.
    ws_.control_callback(
        std::bind(
            &websocket_session::on_control_callback,
            this,
            std::placeholders::_1,
            std::placeholders::_2));

    // Run the timer. The timer is operated
    // continuously, this simplifies the code.
    on_timer({});

    // Set the timer
    timer_.expires_after(std::chrono::seconds(15));

    // Accept the websocket handshake
    ws_.async_accept(
        req,
        boost::asio::bind_executor(
            strand_,
            std::bind(
                &websocket_session::on_accept,
                shared_from_this(),
                std::placeholders::_1)));
}


// Handles an HTTP server connection
class http_session : public std::enable_shared_from_this<http_session>
{
    // This queue is used for HTTP pipelining.
    class queue {
        enum {
            // Maximum number of responses we will queue
            limit = 8
        };

        // The type-erased, saved work item
        struct work {
            virtual ~work() = default;
            virtual void operator()() = 0;
        };

        http_session& self_;
        std::vector<std::unique_ptr<work>> items_;

    public:
        explicit
            queue(http_session& self)
            : self_(self) {
            static_assert(limit > 0, "queue limit must be positive");
            items_.reserve(limit);
        }

        // Returns `true` if we have reached the queue limit
        bool is_full() const {
            return items_.size() >= limit;
        }

        // Called when a message finishes sending
        // Returns `true` if the caller should initiate a read
        bool on_write() {
            BOOST_ASSERT(!items_.empty());
            auto const was_full = is_full();
            items_.erase(items_.begin());
            if (!items_.empty())
                (*items_.front())();
            return was_full;
        }

        // Called by the HTTP handler to send a response.
        template<bool isRequest, class Body, class Fields>
        void operator()(http::message<isRequest, Body, Fields>&& msg) {
            // This holds a work item
            struct work_impl : work
            {
                http_session& self_;
                http::message<isRequest, Body, Fields> msg_;

                work_impl(
                    http_session& self,
                    http::message<isRequest, Body, Fields>&& msg)
                    : self_(self)
                    , msg_(std::move(msg))
                {
                }

                void
                    operator()()
                {
                    http::async_write(
                        self_.socket_,
                        msg_,
                        boost::asio::bind_executor(
                            self_.strand_,
                            std::bind(
                                &http_session::on_write,
                                self_.shared_from_this(),
                                std::placeholders::_1,
                                msg_.need_eof())));
                }
            };

            // Allocate and store the work
            items_.push_back(
                boost::make_unique<work_impl>(self_, std::move(msg)));

            // If there was no previous work, start this one
            if (items_.size() == 1)
                (*items_.front())();
        }
    };

    tcp::socket socket_;
    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;
    boost::asio::steady_timer timer_;
    boost::beast::flat_buffer buffer_;
    std::string doc_root_;
    http::request<http::string_body> req_;
    queue queue_;
    std::shared_ptr<websocket_session> ws;
    bool closed;
public:
    // Take ownership of the socket
    explicit http_session(
        tcp::socket socket,
        std::string doc_root);

    auto getWebsocketSession() {
        return ws;
    }

    // Start the asynchronous operation
    void run();

    void do_read();

    // Called when the timer expires.
    void on_timer(boost::system::error_code ec);

    void on_read(boost::system::error_code ec);

    void on_write(boost::system::error_code ec, bool close);

    void do_close();

    bool isClosed() {
        return closed;
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener> {
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::string doc_root_;
    std::vector<std::shared_ptr<http_session>> sessions;

public:
    const std::vector<std::shared_ptr<http_session>>& getSessions() {
        return sessions;
    }

    listener(
        boost::asio::io_context& ioc,
        tcp::endpoint endpoint,
        std::string doc_root);

    // Start accepting incoming connections
    void run();
    void do_accept();
    void on_accept(boost::system::error_code ec);
    void cleanupClosed();
};

class Server {

public:
    Server();

    boost::asio::io_context ioc{ 1 };
    std::vector<std::thread> iothreads;
    std::shared_ptr<listener> httpServ;
};
