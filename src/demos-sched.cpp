#include "majorframe.hpp"
#include "config_parser.hpp"
#include <list>
#include <iostream>

using namespace std::chrono_literals;

void print_help()
{
    printf("help:\n"
            "-h\n"
            "\t print this message\n"
            "-g <NAME>\n"
            "\t name of cgroup with user access,\n"
            "\t need to create /sys/fs/cgroup/freezer/<NAME> and\n"
            "\t /sys/fs/cgroup/cpuset/<NAME> manually, then\n"
            "\t sudo chown -R <user> .../<NAME>\n"
            "\t if not set, \"my_cgroup\" is used\n"
            "TODO -c <FILE>\n"
            "\t path to configuration file\n");
}

using namespace std;

int main(int argc, char *argv[])
{
    ev::default_loop loop;

    try {
        ConfigParser parser("../src/config.yaml");

        shared_ptr<MajorFrame> mf_ptr;
        parser.parse(loop, mf_ptr);

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;

    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
    }

    return 0;
}
