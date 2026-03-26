#include <common/config_manager.hpp>
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
        infra::config::ConfigManager::getInstance().loadConfig(config_file);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::string log_dir = infra::config::ConfigManager::getInstance().getValue<std::string>("log_dir");
    std::cout << "Log directory: " << log_dir << std::endl;
    return EXIT_SUCCESS;
}