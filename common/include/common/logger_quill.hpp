#ifndef COMMON_LOGGER_QUILL_HPP
#define COMMON_LOGGER_QUILL_HPP

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/FileSink.h>
#include <stdexcept>
#include <string>

namespace infra
{
    namespace common
    {
        class QuillLogger
        {
        public:
            static QuillLogger &getInstance(const std::string &log_path = "")
            {
                static QuillLogger instance(log_path);
                return instance;
            }

            quill::Logger *get() const { return logger_; }

            QuillLogger(const QuillLogger &) = delete;
            QuillLogger &operator=(const QuillLogger &) = delete;
            QuillLogger(QuillLogger &&) = delete;
            QuillLogger &operator=(QuillLogger &&) = delete;

        private:
            explicit QuillLogger(const std::string &log_path)
            {
                if (log_path.empty())
                    throw std::runtime_error("QuillLogger: Log path must be provided on first call");

                quill::BackendOptions backend_options;
                backend_options.thread_name = "QuillLoggerBackend";
                quill::Backend::start(backend_options);

                quill::FileSinkConfig sink_config;
                sink_config.set_open_mode('a');
                sink_config.set_filename_append_option(quill::FilenameAppendOption::StartDateTime);

                auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(log_path, sink_config);

                quill::PatternFormatterOptions fmt;
                fmt.format_pattern =
                    "%(time) [%(log_level)] [%(logger)] %(message) "
                    "%(caller_function) @ %(short_source_location)";
                fmt.timestamp_pattern = "%Y-%m-%d %H:%M:%S.%Qus";

                logger_ = quill::Frontend::create_or_get_logger("infra", std::move(file_sink), fmt);
                logger_->set_log_level(quill::LogLevel::Debug);
            }

            quill::Logger *logger_ = nullptr;
        };

        inline quill::Logger *getLogger()
        {
            return QuillLogger::getInstance().get();
        }
    }
}

#define INFRA_LOG_DEBUG(msg, ...) LOG_DEBUG(infra::common::getLogger(), msg __VA_OPT__(, ) __VA_ARGS__)
#define INFRA_LOG_INFO(msg, ...) LOG_INFO(infra::common::getLogger(), msg __VA_OPT__(, ) __VA_ARGS__)
#define INFRA_LOG_WARN(msg, ...) LOG_WARNING(infra::common::getLogger(), msg __VA_OPT__(, ) __VA_ARGS__)
#define INFRA_LOG_ERROR(msg, ...) LOG_ERROR(infra::common::getLogger(), msg __VA_OPT__(, ) __VA_ARGS__)
#define INFRA_LOG_CRITICAL(msg, ...) LOG_CRITICAL(infra::common::getLogger(), msg __VA_OPT__(, ) __VA_ARGS__)

#endif // COMMON_LOGGER_QUILL_HPP