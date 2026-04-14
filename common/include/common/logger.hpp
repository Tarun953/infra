#ifndef INFRA_COMMON_LOGGER_HPP
#define INFRA_COMMON_LOGGER_HPP

#include <containers/spsc_queue.hpp>
#include <iostream>
#include <atomic>
#include <string>
#include <thread>
#include <fstream>

namespace infra
{
    namespace common
    {
        class Logger
        {
        private:
            containers::SPSCQueue<std::string, 65536> log_queue_{};
            std::string log_directory_{};
            std::thread log_thread_;
            std::ofstream log_file_;
            std::atomic<bool> running_{false};

            Logger() = default;

            void run()
            {
                std::string log_entry;
                log_entry.reserve(1024); // Reserve space to avoid frequent reallocations
                while (running_.load(std::memory_order_acquire) || !log_queue_.empty())
                {
                    if (log_queue_.pop(log_entry))
                    {
                        // remove std::endl to avoid flushing the stream on every log entry, which can significantly improve performance
                        log_file_ << log_entry << "\n";
                    }
                }
            }

        public:
            static Logger &getInstance()
            {
                static Logger instance;
                return instance;
            }

            void initialize(const std::string &log_directory, const std::string &log_file_name = "app.log", bool overwrite = false)
            {
                if (log_directory_ != "")
                {
                    return; // Logger is already initialized
                }
                log_directory_ = log_directory;
                log_file_.open(log_directory_ + "/" + log_file_name, overwrite ? std::ios::out : std::ios::out | std::ios::app);
                if (!log_file_.is_open())
                {
                    throw std::runtime_error("Failed to open log file");
                }
                running_.store(true, std::memory_order_release);
                log_thread_ = std::thread(&Logger::run, this);
            }

            template <typename... Args>
            void log(Args &&...args)
            {
                std::ostringstream oss;
                (oss << ... << std::forward<Args>(args)); // Fold expression to concatenate all arguments into a single string
                while (!log_queue_.push(std::move(oss.str())))
                    ;
            }

            ~Logger()
            {
                running_.store(false, std::memory_order_release);
                if (log_thread_.joinable())
                    log_thread_.join();
                if (log_file_.is_open())
                    log_file_.close();
                std::cout << "[Logger] Logger destroyed" << std::endl;
            }
        };
    }
}

#endif // INFRA_COMMON_LOGGER_HPP