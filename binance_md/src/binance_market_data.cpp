#include <common/config_manager.hpp>
#include <common/logger.hpp>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    if (argc < 1)
    {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string config_file = std::string(argv[1]);

    try
    {
        infra::common::ConfigManager::getInstance().loadConfig(config_file);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::string log_dir = infra::common::ConfigManager::getInstance().getValue<std::string>("log_dir");
    std::string log_file_name = infra::common::ConfigManager::getInstance().getValue<std::string>("log_file_name");

    infra::common::Logger::getInstance().initialize(log_dir, log_file_name);
    infra::common::Logger::getInstance().log("Application started");
    infra::common::Logger::getInstance().log("Configuration loaded from: " + config_file);
    infra::common::Logger::getInstance().log("Log directory: " + log_dir);
    infra::common::Logger::getInstance().log("Log file name: " + log_file_name);
    infra::common::Logger::getInstance().log("Application closing");

    return EXIT_SUCCESS;
}