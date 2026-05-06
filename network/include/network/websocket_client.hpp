#ifndef NETWORK_WEBSOCKET_CLIENT_HPP
#define NETWORK_WEBSOCKET_CLIENT_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <functional>
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

            WebSocketClient(boost::asio::io_context &ioc, boost::asio::ssl::context &sslContext, Config config) : resolver_(boost::asio::make_strand(ioc)), ws_(boost::asio::make_strand(ioc), sslContext), config_(std::move(config))
            {
                buffer_.prepare(config_.bufferSize_);
            }

            WebSocketClient(boost::asio::io_context &ioc, Config config) : resolver_(boost::asio::make_strand(ioc)), wsPlain_(boost::asio::make_strand(ioc)), config_(std::move(config))
            {
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

        private:
            boost::asio::ip::tcp::resolver resolver_;                                                // For resolving the host and port
            boost::beast::websocket::stream<boost::beast::tcp_stream> wsPlain_;                      // WebSocket stream without SSL support
            boost::beast::websocket::stream<boost::asio::ssl::stream<boost::beast::tcp_stream>> ws_; // WebSocket stream with SSL support
            boost::beast::flat_buffer buffer_;                                                       // Buffer for incoming messages
            Config config_;                                                                          // Configuration for the WebSocket client
            bool useSSL_ = true;                                                                     // Flag to indicate whether to use SSL or not

            MessageCallBack messageCallBack_; // Callback for when a message is received
            ErrorCallBack errorCallBack_;     // Callback for when an error occurs
            ConnectCallBack connectCallBack_; // Callback for when the connection is established

            boost::asio::strand<boost::asio::io_context::executor_type> write_strand_; // Strand to ensure thread-safe writes
            std::queue<std::string> writeQueue_;                                       // Queue for outgoing messages
            std::atomic<bool> connected_{false};                                       // Atomic flag to indicate connection status
        };
    }
}

#endif // NETWORK_WEBSOCKET_CLIENT_HPP