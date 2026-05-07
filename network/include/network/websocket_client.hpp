#ifndef NETWORK_WEBSOCKET_CLIENT_HPP
#define NETWORK_WEBSOCKET_CLIENT_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <functional>
#include <memory>
#include <string>
#include <atomic>
#include <queue>

namespace infra
{
    namespace network
    {
        class WebSocketClient : public std::enable_shared_from_this<WebSocketClient>
        {
        public:
            using MessageCallBack = std::function<void(const char *data, std::size_t size)>;
            using ErrorCallBack = std::function<void(const std::string &error)>;
            using ConnectCallBack = std::function<void()>;

            struct Config
            {
                std::string host_;               // This is the hostname or IP address of the WebSocket server like "example.com" or "127.0.0.1"
                std::string port_;               // port number as a string, for example "80" for non-secure WebSocket or "443" for secure WebSocket (wss)
                std::string target_;             // This is the target path for the WebSocket connection, for example "/ws"
                bool useSSL_ = true;             // To switch between wss:// or ws://
                std::size_t bufferSize_ = 65536; // Size of the buffer for reading messages, default is 64KB
                bool tcpNoDelay_ = true;         // To disable Nagle's algorithm for lower latency
                int sendBufferSize_ = 262144;    // SO_SNDBUF, Size of the send buffer, default is 64KB
                int receiveBufferSize_ = 262144; // SO_RCVBUF, Size of the receive buffer, default is 64KB
                bool reuseAddress_ = true;       // To allow reuse of local addresses
            };

            WebSocketClient(boost::asio::io_context &ioc, boost::asio::ssl::context &sslContext, Config config) : resolver_(boost::asio::make_strand(ioc)), config_(std::move(config)), writeStrand_(boost::asio::make_strand(ioc)), useSSL_(true)
            {
                ws_ = std::make_unique<boost::beast::websocket::stream<boost::asio::ssl::stream<boost::beast::tcp_stream>>>(boost::asio::make_strand(ioc), sslContext);
                buffer_.prepare(config_.bufferSize_);
            }

            WebSocketClient(boost::asio::io_context &ioc, Config config) : resolver_(boost::asio::make_strand(ioc)), config_(std::move(config)), writeStrand_(boost::asio::make_strand(ioc)), useSSL_(false)
            {
                wsPlain_ = std::make_unique<boost::beast::websocket::stream<boost::beast::tcp_stream>>(boost::asio::make_strand(ioc));
                buffer_.prepare(config_.bufferSize_);
            }

            void setMessageCallBack(MessageCallBack callback)
            {
                messageCallBack_ = std::move(callback);
            }

            void setErrorCallBack(ErrorCallBack callback)
            {
                errorCallBack_ = std::move(callback);
            }

            void setConnectCallBack(ConnectCallBack callback)
            {
                connectCallBack_ = std::move(callback);
            }

            // Start the asynchronous connection process
            void connect()
            {
                // Start the asynchronous resolve operation to translate the server and service names into a list of endpoints.
                // The bind_front_handler is used to bind the on_resolve member function as the handler for when the resolve operation completes.
                resolver_.async_resolve(config_.host_, config_.port_, boost::beast::bind_front_handler(&WebSocketClient::on_resolve, shared_from_this()));
            }

            // Send a message asynchronously. The message is added to the write queue and the write operation is initiated if the queue was previously empty.
            void send(std::string message)
            {
                /*
                    Post the write operation to the strand to ensure that all write operations are executed in a thread-safe manner.
                    The lambda captures the shared pointer to the WebSocketClient instance and the message to be sent.
                    It pushes the message onto the write queue and if the queue was empty before adding the new message, it starts the write operation by calling do_write().
                 */
                boost::asio::post(writeStrand_, [self = shared_from_this(), msg = std::move(message)]() mutable
                                  { self->writeQueue_.push(std::move(msg)); 
                                    if(self->writeQueue_.size() == 1) // If the queue was empty before adding the new message, start the write operation
                                        self->do_write(); });
            }

            /*
                This function initiates the asynchronous close operation for the WebSocket connection. It checks whether SSL is being used and calls the appropriate async_close function on the WebSocket stream.
                The async_close function takes a close code (in this case, boost::beast::websocket::close_code::normal) and a handler that will be called when the close operation completes. The handler is bound to the on_close member function of the WebSocketClient class.
            */
            void close()
            {
                if (useSSL_)
                {
                    ws_->async_close(boost::beast::websocket::close_code::normal, boost::beast::bind_front_handler(&WebSocketClient::on_close, shared_from_this()));
                }
                else
                {
                    wsPlain_->async_close(boost::beast::websocket::close_code::normal, boost::beast::bind_front_handler(&WebSocketClient::on_close, shared_from_this()));
                }
            }

            /*
                This function returns the connection status of the WebSocket client. It uses an atomic boolean variable connected_ to track whether the client is currently connected to the server.
                The function returns true if the client is connected and false otherwise. The memory order for the atomic load operation is set to std::memory_order_acquire to ensure that any changes made to the connection status are visible to other threads that may be checking the connection status concurrently.
            */
            bool is_connected() const
            {
                return connected_.load(std::memory_order_acquire);
            }

        private:
            /*
                This function is the handler for when an asynchronous resolve operation completes. It takes a boost::beast::error_code and a list of resolved endpoints as parameters.
                If there is an error (indicated by a non-zero error code), it calls the fail function to handle the error and returns immediately.
                If the resolve operation was successful, it initiates an asynchronous connect operation to the resolved endpoints. The connect operation also has a timeout set for faster failure detection.
            */
            void on_resolve(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results)
            {
                if (ec)
                {
                    fail(ec, "Resolve");
                    return;
                }

                // Connect with timeout for faster failure detection
                // boost::beast::get_lowest_layer(useSSL_ ? ws_.next_layer() : wsPlain_.next_layer()).expires_after(std::chrono::seconds(30));

                if (useSSL_)
                {
                    boost::beast::get_lowest_layer(ws_->next_layer()).expires_after(std::chrono::seconds(30));
                    boost::beast::get_lowest_layer(ws_->next_layer()).async_connect(results, boost::beast::bind_front_handler(&WebSocketClient::on_connect, shared_from_this()));
                }
                else
                {
                    boost::beast::get_lowest_layer(*wsPlain_).expires_after(std::chrono::seconds(30));
                    boost::beast::get_lowest_layer(*wsPlain_).async_connect(results, boost::beast::bind_front_handler(&WebSocketClient::on_connect_plain, shared_from_this()));
                }
            }

            void on_connect(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type ep)
            {
                if (ec)
                {
                    fail(ec, "Connect");
                    return;
                }

                apply_socket_options(boost::beast::get_lowest_layer(ws_->next_layer()).socket());
                boost::beast::get_lowest_layer(ws_->next_layer()).expires_after(std::chrono::seconds(30));
                ws_->next_layer().async_handshake(boost::asio::ssl::stream_base::client, boost::beast::bind_front_handler(&WebSocketClient::on_ssl_handshake, shared_from_this()));
            }

            void on_connect_plain(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type ep)
            {
                if (ec)
                {
                    fail(ec, "Connect");
                    return;
                }

                apply_socket_options(boost::beast::get_lowest_layer(*wsPlain_).socket());
                perform_websocket_handshake_plain();
            }

            void on_ssl_handshake(boost::beast::error_code ec)
            {
                if (ec)
                {
                    fail(ec, "SSL Handshake");
                    return;
                }

                perform_websocket_handshake();
            }

            void perform_websocket_handshake()
            {
                // Set a timeout for the WebSocket handshake to prevent hanging indefinitely if the server does not respond. The expires_never() function is used to disable the timeout after the handshake is complete.
                boost::beast::get_lowest_layer(ws_->next_layer()).expires_never();

                // Set suggested timeout settings for the websocket stream based on whether it's a client or server connection. This helps to ensure that the connection is properly managed and that timeouts are handled appropriately.
                ws_->set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));

                // Set a decorator to change the User-Agent of the handshake. This allows the client to identify itself to the server with a custom User-Agent string, which can be useful for debugging or for servers that require specific User-Agent values.
                ws_->set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type &req)
                                                                                { req.set(boost::beast::http::field::user_agent, std::string("WebSocketClient/1.0")); }));

                /*
                    Perform the asynchronous WebSocket handshake. The async_handshake function initiates the handshake process with the server using the specified host and target from the configuration.
                    The handler for when the handshake completes is bound to the on_handshake member function of the WebSocketClient class. This function will be called once the handshake is complete, allowing the client to proceed with sending and receiving messages over the WebSocket connection.
                */
                ws_->async_handshake(config_.host_, config_.target_, boost::beast::bind_front_handler(&WebSocketClient::on_handshake, shared_from_this()));
            }

            void perform_websocket_handshake_plain()
            {
                // Set a timeout for the WebSocket handshake to prevent hanging indefinitely if the server does not respond. The expires_never() function is used to disable the timeout after the handshake is complete.
                boost::beast::get_lowest_layer(*wsPlain_).expires_never();

                // Set suggested timeout settings for the websocket stream based on whether it's a client or server connection. This helps to ensure that the connection is properly managed and that timeouts are handled appropriately.
                wsPlain_->set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));

                // Set a decorator to change the User-Agent of the handshake. This allows the client to identify itself to the server with a custom User-Agent string, which can be useful for debugging or for servers that require specific User-Agent values.
                wsPlain_->set_option(boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type &req)
                                                                                     { req.set(boost::beast::http::field::user_agent, std::string("WebSocketClient/1.0")); }));

                /*
                    Perform the asynchronous WebSocket handshake. The async_handshake function initiates the handshake process with the server using the specified host and target from the configuration.
                    The handler for when the handshake completes is bound to the on_handshake member function of the WebSocketClient class. This function will be called once the handshake is complete, allowing the client to proceed with sending and receiving messages over the WebSocket connection.
                */
                wsPlain_->async_handshake(config_.host_, config_.target_, boost::beast::bind_front_handler(&WebSocketClient::on_handshake, shared_from_this()));
            }

            void on_handshake(boost::beast::error_code ec)
            {
                if (ec)
                {
                    fail(ec, "Handshake");
                    return;
                }

                connected_.store(true, std::memory_order_release);
                if (connectCallBack_)
                {
                    connectCallBack_();
                }

                do_read();
            }

            // This function applies socket options to the underlying TCP socket. It takes a reference to a boost::asio::ip::tcp::socket as a parameter and sets various options based on the configuration of the WebSocket client.
            void apply_socket_options(boost::asio::ip::tcp::socket &socket)
            {
                boost::beast::error_code ec;
                // Disable Nagle's algorithm if specified in the configuration
                if (config_.tcpNoDelay_)
                {
                    socket.set_option(boost::asio::ip::tcp::no_delay(true), ec);
                }

                // Set send and receive buffer sizes if specified in the configuration
                socket.set_option(boost::asio::socket_base::send_buffer_size(config_.sendBufferSize_), ec);
                socket.set_option(boost::asio::socket_base::receive_buffer_size(config_.receiveBufferSize_), ec);

                // Allow reuse of local addresses if specified in the configuration. This can be useful for quickly restarting the client without waiting for the OS to release the previous socket.
                if (config_.reuseAddress_)
                {
                    socket.set_option(boost::asio::socket_base::reuse_address(true), ec);
                }

                socket.set_option(boost::asio::socket_base::keep_alive(true), ec);
            }

            /*
                This function is the handler for when an asynchronous close operation completes. It takes a boost::beast::error_code as a parameter.
                It first sets the connected_ atomic flag to false to indicate that the connection is closed.
                If there is an error (indicated by a non-zero error code), it calls the fail function to handle the error and returns immediately.
            */
            void on_close(boost::beast::error_code ec)
            {
                connected_.store(false, std::memory_order_release);
                if (ec)
                {
                    fail(ec, "Close");
                }
            }

            /*
                This function is responsible for initiating the asynchronous write operation to send a message from the write queue.
                It first checks if the write queue is empty, and if it is, it returns immediately since there are no messages to send.
                If there are messages in the queue, it takes the front message and initiates an asynchronous write operation using either the SSL or non-SSL WebSocket stream depending on the configuration.
                The async_write function takes a buffer containing the message and a handler that will be called when the write operation completes. The handler is bound to the on_write member function of the WebSocketClient class.
            */
            void do_write()
            {
                if (writeQueue_.empty())
                    return;

                auto &message = writeQueue_.front();
                if (useSSL_)
                {
                    ws_->async_write(boost::asio::buffer(message), boost::beast::bind_front_handler(&WebSocketClient::on_write, shared_from_this()));
                }
                else
                {
                    wsPlain_->async_write(boost::asio::buffer(message), boost::beast::bind_front_handler(&WebSocketClient::on_write, shared_from_this()));
                }
            }

            /*
                This function is the handler for when an asynchronous write operation completes. It takes a boost::beast::error_code and the number of bytes transferred as parameters.
                If there is an error (indicated by a non-zero error code), it calls the fail function to handle the error and returns immediately.
                If the write operation was successful, it pops the front message from the write queue since it has been sent.
                Then, it checks if there are more messages in the queue, and if there are, it calls do_write() again to send the next message.
            */
            void on_write(boost::beast::error_code ec, std::size_t bytes_transferred)
            {
                if (ec)
                {
                    fail(ec, "Write");
                    return;
                }
                writeQueue_.pop();
                if (!writeQueue_.empty())
                {
                    do_write();
                }
            }

            /*
                This function initiates the asynchronous read operation to receive messages from the WebSocket connection.
                It checks whether SSL is being used and calls the appropriate async_read function on the WebSocket stream.
                The async_read function takes a buffer to store the incoming message and a handler that will be called when the read operation completes. The handler is bound to the on_read member function of the WebSocketClient class.
            */
            void do_read()
            {
                if (useSSL_)
                {
                    ws_->async_read(buffer_, boost::beast::bind_front_handler(&WebSocketClient::on_read, shared_from_this()));
                }
                else
                {
                    wsPlain_->async_read(buffer_, boost::beast::bind_front_handler(&WebSocketClient::on_read, shared_from_this()));
                }
            }

            /*
                This function is the handler for when an asynchronous read operation completes. It takes a boost::beast::error_code and the number of bytes transferred as parameters.
                If there is an error (indicated by a non-zero error code), it calls the fail function to handle the error and returns immediately.
                If the read operation was successful, it calls the message callback with the received message.
            */
            void on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
            {
                if (ec)
                {
                    fail(ec, "Read");
                    return;
                }

                if (messageCallBack_)
                {
                    const char *data = static_cast<const char *>(buffer_.data().data());
                    messageCallBack_(data, bytes_transferred);
                }

                buffer_.consume(bytes_transferred); // Remove the consumed data from the buffer
                do_read();
            }

            /*
                This function is a utility function for handling errors. It takes a boost::beast::error_code and a string describing the context of the error (e.g., "Resolve", "Connect", "Write", etc.).
                It first sets the connected_ atomic flag to false to indicate that the connection is no longer active.
                If an error callback has been set, it calls the error callback with a formatted error message that includes the context and the error message from the error code.
            */
            void fail(boost::beast::error_code ec, char const *what)
            {
                connected_.store(false, std::memory_order_release);
                if (errorCallBack_)
                {
                    errorCallBack_(std::string(what) + ": " + ec.message());
                }
            }

            boost::asio::ip::tcp::resolver resolver_;                                                                 // For resolving the host and port
            std::unique_ptr<boost::beast::websocket::stream<boost::beast::tcp_stream>> wsPlain_;                      // WebSocket stream without SSL support
            std::unique_ptr<boost::beast::websocket::stream<boost::asio::ssl::stream<boost::beast::tcp_stream>>> ws_; // WebSocket stream with SSL support
            boost::beast::flat_buffer buffer_;                                                                        // Buffer for incoming messages
            Config config_;                                                                                           // Configuration for the WebSocket client
            bool useSSL_ = true;                                                                                      // Flag to indicate whether to use SSL or not

            MessageCallBack messageCallBack_; // Callback for when a message is received
            ErrorCallBack errorCallBack_;     // Callback for when an error occurs
            ConnectCallBack connectCallBack_; // Callback for when the connection is established

            boost::asio::strand<boost::asio::io_context::executor_type> writeStrand_; // Strand to ensure thread-safe writes
            std::queue<std::string> writeQueue_;                                      // Queue for outgoing messages
            std::atomic<bool> connected_{false};                                      // Atomic flag to indicate connection status
        };
    }
}

#endif // NETWORK_WEBSOCKET_CLIENT_HPP