#include "websocket.hpp"

extern std::mutex frameLock;

std::set<std::shared_ptr<websocket_session>> wsSessions;

// Return a reasonable mime type based on the extension of a file.
boost::beast::string_view
mime_type(boost::beast::string_view path)
{
    using boost::beast::iequals;
    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if (pos == boost::beast::string_view::npos)
            return boost::beast::string_view{};
        return path.substr(pos);
    }();
    if (iequals(ext, ".htm"))  return "text/html";
    if (iequals(ext, ".html")) return "text/html";
    if (iequals(ext, ".php"))  return "text/html";
    if (iequals(ext, ".css"))  return "text/css";
    if (iequals(ext, ".txt"))  return "text/plain";
    if (iequals(ext, ".js"))   return "application/javascript";
    if (iequals(ext, ".json")) return "application/json";
    if (iequals(ext, ".xml"))  return "application/xml";
    if (iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if (iequals(ext, ".flv"))  return "video/x-flv";
    if (iequals(ext, ".png"))  return "image/png";
    if (iequals(ext, ".jpe"))  return "image/jpeg";
    if (iequals(ext, ".jpeg")) return "image/jpeg";
    if (iequals(ext, ".jpg"))  return "image/jpeg";
    if (iequals(ext, ".gif"))  return "image/gif";
    if (iequals(ext, ".bmp"))  return "image/bmp";
    if (iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff")) return "image/tiff";
    if (iequals(ext, ".tif"))  return "image/tiff";
    if (iequals(ext, ".svg"))  return "image/svg+xml";
    if (iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
    boost::beast::string_view base,
    boost::beast::string_view path)
{
    if (base.empty())
        return path.to_string();
    std::string result = base.to_string();
#if BOOST_MSVC
    char constexpr path_separator = '\\';
    if (result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for (auto& c : result)
        if (c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if (result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<
    class Body, class Allocator,
    class Send>
    void
    handle_request(
        boost::beast::string_view doc_root,
        http::request<Body, http::basic_fields<Allocator>>&& req,
        Send&& send)
{
    // Returns a bad request response
    auto const bad_request =
        [&req](boost::beast::string_view why)
    {
        http::response<http::string_body> res{ http::status::bad_request, req.version() };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = why.to_string();
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found =
        [&req](boost::beast::string_view target)
    {
        http::response<http::string_body> res{ http::status::not_found, req.version() };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + target.to_string() + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error =
        [&req](boost::beast::string_view what)
    {
        http::response<http::string_body> res{ http::status::internal_server_error, req.version() };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + what.to_string() + "'";
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if (req.method() != http::verb::get &&
        req.method() != http::verb::head)
        return send(bad_request("Unknown HTTP-method"));

    // Request path must be absolute and not contain "..".
    if (req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != boost::beast::string_view::npos)
        return send(bad_request("Illegal request-target"));

    // Build the path to the requested file
    std::string path = path_cat(doc_root, req.target());
    if (req.target().back() == '/')
        path.append("index.html");

    // Attempt to open the file
    boost::beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(), boost::beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if (ec == boost::system::errc::no_such_file_or_directory)
        return send(not_found(req.target()));

    // Handle an unknown error
    if (ec)
        return send(server_error(ec.message()));

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to HEAD request
    if (req.method() == http::verb::head)
    {
        http::response<http::empty_body> res{ http::status::ok, req.version() };
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }

    // Respond to GET request
    http::response<http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, req.version()) };
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
}



//------------------------------------------------------------------------------

std::string thisDllDirPath()
{
    std::string thisPath = "";
    CHAR path[MAX_PATH];
    HMODULE hm;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPSTR)& thisDllDirPath, &hm))
    {
        GetModuleFileNameA(hm, path, sizeof(path));
        thisPath = std::string(path);
    }
    return thisPath;
}


void fail(boost::system::error_code ec, char const* what) {
    std::cerr << what << ": " << ec.message() << "\n";
}

websocket_session::websocket_session(tcp::socket socket): ws_(std::move(socket))
    , strand_(ws_.get_executor())
    , timer_(ws_.get_executor().context(),
        (std::chrono::steady_clock::time_point::max)()) {}

void websocket_session::on_accept(boost::system::error_code ec) {
    // Happens when the timer closes the socket
    if (ec == boost::asio::error::operation_aborted)
        return;

    if (ec)
        return fail(ec, "accept");

    // Read a message
    do_read();
}

void websocket_session::on_timer(boost::system::error_code ec) {
    if (ec && ec != boost::asio::error::operation_aborted)
        return fail(ec, "timer");

    // See if the timer really expired since the deadline may have moved.
    if (timer_.expiry() <= std::chrono::steady_clock::now()) {
        // If this is the first time the timer expired,
        // send a ping to see if the other end is there.
        if (ws_.is_open() && ping_state_ == 0) {
            // Note that we are sending a ping
            ping_state_ = 1;

            // Set the timer
            timer_.expires_after(std::chrono::seconds(15));

            // Now send the ping
            ws_.async_ping({},
                boost::asio::bind_executor(
                    strand_,
                    std::bind(
                        &websocket_session::on_ping,
                        shared_from_this(),
                        std::placeholders::_1)));
        } else {
            // The timer expired while trying to handshake,
            // or we sent a ping and it never completed or
            // we never got back a control frame, so close.

            // Closing the socket cancels all outstanding operations. They
            // will complete with boost::asio::error::operation_aborted
            ws_.next_layer().shutdown(tcp::socket::shutdown_both, ec);
            ws_.next_layer().close(ec);
            return;
        }
    }

    // Wait on the timer
    timer_.async_wait(
        boost::asio::bind_executor(
            strand_,
            std::bind(
                &websocket_session::on_timer,
                shared_from_this(),
                std::placeholders::_1)));
}

void websocket_session::activity() {
    // Note that the connection is alive
    ping_state_ = 0;

    // Set the timer
    timer_.expires_after(std::chrono::seconds(15));
}

void websocket_session::on_ping(boost::system::error_code ec) {
    // Happens when the timer closes the socket
    if (ec == boost::asio::error::operation_aborted)
        return;

    if (ec)
        return fail(ec, "ping");

    // Note that the ping was sent.
    if (ping_state_ == 1) {
        ping_state_ = 2;
    } else {
        // ping_state_ could have been set to 0
        // if an incoming control frame was received
        // at exactly the same time we sent a ping.
        BOOST_ASSERT(ping_state_ == 0);
    }
}

void websocket_session::on_control_callback(websocket::frame_type kind, boost::beast::string_view payload) {
    boost::ignore_unused(kind, payload);

    // Note that there is activity
    activity();
}

void websocket_session::do_read() {
    // Read a message into our buffer
    ws_.async_read(
        buffer_,
        boost::asio::bind_executor(
            strand_,
            std::bind(
                &websocket_session::on_read,
                shared_from_this(),
                std::placeholders::_1,
                std::placeholders::_2)));
}

json websocket_session::processTask(const json& task) {
    //#TODO make unordered_map with std::function
    if (task["type"] == "getPlayerlist") {

        std::vector<std::string> unitNames;
        for (auto& it : intercept::sqf::all_players()) {
            unitNames.emplace_back(intercept::sqf::name(it));
        }

        json playerMessage;
        playerMessage["type"] = "playerlist";
        playerMessage["players"] = unitNames;

        return playerMessage;
    } else if (task["type"] == "Exec") {
        auto res = intercept::sqf::call(intercept::sqf::compile(static_cast<std::string_view>(task["script"])));

        json playerMessage;
        playerMessage["type"] = "ExecRet";
        playerMessage["res"] = static_cast<std::string>(res);
        if (task.find("watch") != task.end())
            playerMessage["watch"] = task["watch"];

        return playerMessage;
    } else if (task["type"] == "ExecFunc") {
        auto func = intercept::sqf::get_variable(intercept::sqf::mission_namespace(),
            static_cast<std::string_view>(task["fnc"]));

        auto_array<game_value> args;
        for (auto& it : task["args"]) {
            if (it.is_number())
                args.emplace_back(static_cast<float>(it));
            else if (it.is_string())
                args.emplace_back(static_cast<std::string_view>(it));
            else if (it.is_boolean())
                args.emplace_back(static_cast<bool>(it));
            else if (it.is_object())
                args.emplace_back(intercept::sqf::compile(static_cast<std::string_view>(it["code"])));
        }

        auto res = intercept::sqf::call(func, args);

        json playerMessage;
        playerMessage["type"] = "ExecRet";
        playerMessage["res"] = static_cast<std::string>(res);
        if (task.find("watch") != task.end())
            playerMessage["watch"] = task["watch"];
        return playerMessage;
    }
    return {};
}

void websocket_session::processTasks() {
    taskMutex.lock(); 
    for (auto& it : todoTasks) {
        completedTasks.emplace_back(doTask(it));
    }
    bool tasksCompleted = !todoTasks.empty();
    todoTasks.clear();
    taskMutex.unlock();
    if (tasksCompleted) {
        return boost::asio::post(
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &websocket_session::finishTasks,
                    shared_from_this())));
    }
}

void websocket_session::on_read(boost::system::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // Happens when the timer closes the socket
    if (ec == boost::asio::error::operation_aborted)
        return;

    // This indicates that the websocket_session was closed
    if (ec == websocket::error::closed) {
        wsSessions.erase(shared_from_this());
        return;
    }
        

    if (ec)
        fail(ec, "read");

    if (!bytes_transferred)
        return;

    // Note that there is activity
    activity();

    // Echo the message

    std::string message(buffers_to_string(buffer_.data()));

    buffer_.consume(buffer_.size()); //clear buffer


    auto task = json::parse(message, nullptr, false);


    taskMutex.lock();

    if (task.is_array()) {
        for (auto& it : task) {
            todoTasks.emplace_back(Task{it, ws_.got_text()});
        }
    } else {
        todoTasks.emplace_back(Task{task, ws_.got_text()});
    }
    taskMutex.unlock();

}

void websocket_session::finishTasks() {
    json result;
    taskMutex.lock();
    for (auto& it : completedTasks) {
        result.emplace_back(it.message);
    }
    completedTasks.clear();
    taskMutex.unlock();
    std::string msg = result.dump();
    buffer_.consume(buffer_.size()); //clear buffer
    auto n = buffer_copy(buffer_.prepare(msg.size()), boost::asio::buffer(msg));
    buffer_.commit(n);

    ws_.async_write(
        buffer_.data(),
        std::bind(
            &websocket_session::on_write,
            shared_from_this(),
            std::placeholders::_1,
            std::placeholders::_2));
}

void websocket_session::on_write(boost::system::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // Happens when the timer closes the socket
    if (ec == boost::asio::error::operation_aborted)
        return;

    if (ec)
        return fail(ec, "write");

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Do another read
    do_read();
}

http_session::http_session(tcp::socket socket, std::string doc_root): socket_(std::move(socket))
    , strand_(socket_.get_executor())
    , timer_(socket_.get_executor().context(),
        (std::chrono::steady_clock::time_point::max)())
    , doc_root_(doc_root)
    , queue_(*this) {}

void http_session::run() {
    // Make sure we run on the strand
    if (!strand_.running_in_this_thread())
        return boost::asio::post(
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &http_session::run,
                    shared_from_this())));

    // Run the timer. The timer is operated
    // continuously, this simplifies the code.
    on_timer({});

    do_read();
}

void http_session::do_read() {
    // Set the timer
    timer_.expires_after(std::chrono::seconds(15));

    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    req_ = {};

    // Read a request
    http::async_read(socket_, buffer_, req_,
        boost::asio::bind_executor(
            strand_,
            std::bind(
                &http_session::on_read,
                shared_from_this(),
                std::placeholders::_1)));
}

void http_session::on_timer(boost::system::error_code ec) {
    if (ec && ec != boost::asio::error::operation_aborted)
        return fail(ec, "timer");

    // Check if this has been upgraded to Websocket
    if (timer_.expires_at() == (std::chrono::steady_clock::time_point::min)())
        return;

    // Verify that the timer really expired since the deadline may have moved.
    if (timer_.expiry() <= std::chrono::steady_clock::now()) {
        // Closing the socket cancels all outstanding operations. They
        // will complete with boost::asio::error::operation_aborted
        socket_.shutdown(tcp::socket::shutdown_both, ec);
        socket_.close(ec);
        return;
    }

    // Wait on the timer
    timer_.async_wait(
        boost::asio::bind_executor(
            strand_,
            std::bind(
                &http_session::on_timer,
                shared_from_this(),
                std::placeholders::_1)));
}

void http_session::on_read(boost::system::error_code ec) {
    // Happens when the timer closes the socket
    if (ec == boost::asio::error::operation_aborted)
        return;

    // This means they closed the connection
    if (ec == http::error::end_of_stream)
        return do_close();

    if (ec)
        return fail(ec, "read");

    // See if it is a WebSocket Upgrade
    if (websocket::is_upgrade(req_)) {
        // Make timer expire immediately, by setting expiry to time_point::min we can detect
        // the upgrade to websocket in the timer handler
        timer_.expires_at((std::chrono::steady_clock::time_point::min)());

        // Create a WebSocket websocket_session by transferring the socket
        ws = std::make_shared<websocket_session>(
            std::move(socket_));
        ws->do_accept(std::move(req_));
        wsSessions.emplace(ws);
        return;
    }

    // Send the response
    handle_request(doc_root_, std::move(req_), queue_);

    // If we aren't at the queue limit, try to pipeline another request
    if (!queue_.is_full())
        do_read();
}

void http_session::on_write(boost::system::error_code ec, bool close) {
    // Happens when the timer closes the socket
    if (ec == boost::asio::error::operation_aborted)
        return;

    if (ec)
        return fail(ec, "write");

    if (close) {
        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        return do_close();
    }

    // Inform the queue that a write completed
    if (queue_.on_write()) {
        // Read another request
        do_read();
    }
}

void http_session::do_close() {
    // Send a TCP shutdown
    boost::system::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_send, ec);
    closed = true;
    // At this point the connection is closed gracefully
}

listener::listener(boost::asio::io_context& ioc, tcp::endpoint endpoint, std::string doc_root): acceptor_(ioc)
    , socket_(ioc)
    , doc_root_(doc_root) {
    boost::system::error_code ec;

    // Open the acceptor
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        fail(ec, "open");
        return;
    }

    // Allow address reuse
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if (ec) {
        fail(ec, "set_option");
        return;
    }

    // Bind to the server address
    acceptor_.bind(endpoint, ec);
    if (ec) {
        fail(ec, "bind");
        return;
    }

    // Start listening for connections
    acceptor_.listen(
        boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        fail(ec, "listen");
        return;
    }
}

void listener::run() {
    if (!acceptor_.is_open())
        return;
    do_accept();
}

void listener::do_accept() {
    acceptor_.async_accept(
        socket_,
        std::bind(
            &listener::on_accept,
            shared_from_this(),
            std::placeholders::_1));
}

void listener::on_accept(boost::system::error_code ec) {
    if (ec) {
        fail(ec, "accept");
    } else {
        // Create the http_session and run it
        auto newSession = std::make_shared<http_session>(
            std::move(socket_),
            doc_root_);
        sessions.emplace_back(newSession);
        newSession->run();
        cleanupClosed();
    }

    // Accept another connection
    do_accept();
}

void listener::cleanupClosed() {
    sessions.erase(std::remove_if(sessions.begin(), sessions.end(), [](const std::shared_ptr<http_session>& sess)
    {
        return sess->isClosed();
    }), sessions.end());
}

Server::Server() {
    auto const address = net::ip::make_address("0.0.0.0");
    auto const port = static_cast<unsigned short>(8082);

    std::filesystem::path dllPath(thisDllDirPath());

    std::string docroot((dllPath.parent_path() / "wdata").string());

    // Create and launch a listening port
    httpServ =
        std::make_shared<listener>(
            ioc,
            tcp::endpoint{ address, port },
            docroot);
    httpServ->run();

    // Run the I/O service on the requested number of threads
    iothreads.emplace_back( [this] {
            ioc.run();
        });
}
