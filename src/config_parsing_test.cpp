#include "check_lib.hpp"
#include <fstream>
#include <iostream>
#include <list>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cerrno>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;
using namespace std::chrono_literals;

// TODO: is this still relevant?
int main(int argc, char *argv[])
{
    int opt;
    string config_file, config_str;
    while ((opt = getopt(argc, argv, "c:C:")) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                break;
            case 'C':
                config_str = optarg;
                break;
            default:
                exit(1);
        }
    }
    if (config_file.empty() && config_str.empty()) {
        exit(1);
    }

    try {
        YAML::Node config;
        try {
            if (!config_file.empty()) {
                config = YAML::LoadFile(config_file);
            } else if (!config_str.empty()) {
                config = YAML::Load(config_str);
            }
        } catch (const YAML::BadFile &e) {
            throw runtime_error("Cannot load configuration file: " + config_file);
        } catch (const YAML::Exception &e) {
            throw runtime_error("Configuration error: "s + e.what());
        }

        YAML::Node normalized_config;
        normalize_config(config, normalized_config);
        cout << normalized_config << endl;

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;

    } catch (...) {
        cerr << "Unknown exception" << endl;
        return 1;
    }

    return 0;
}
