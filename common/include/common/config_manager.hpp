#include <fkYAML/node.hpp>
#include <fstream>

namespace infra
{
    namespace common
    {
        class ConfigManager
        {
        private:
            ConfigManager() = default;
            fkyaml::node config_root_;
            bool loaded_ = false;

        public:
            static ConfigManager &getInstance()
            {
                static ConfigManager instance;
                return instance;
            }

            void loadConfig(const std::string &config_file)
            {
                std::ifstream file(config_file);
                if (!file.is_open())
                {
                    throw std::runtime_error("Could not open config file: " + config_file);
                    exit(EXIT_FAILURE);
                }
                config_root_ = fkyaml::node::deserialize(file);
                loaded_ = true;
            }

            template <typename T>
            T getValue(const std::string &key) const
            {
                if (!loaded_)
                {
                    throw std::runtime_error("Configuration not loaded. Call loadConfig() first.");
                }
                return config_root_[key].get_value<T>();
            }

            // Delete copy and move constructors and assignment operators
            ConfigManager(const ConfigManager &) = delete;
            ConfigManager(ConfigManager &&) = delete;
            ConfigManager &operator=(const ConfigManager &) = delete;
            ConfigManager &operator=(ConfigManager &&) = delete;
        };
    }
}