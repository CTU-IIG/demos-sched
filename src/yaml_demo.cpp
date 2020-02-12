#include <iostream>
#include <list>
#include <yaml-cpp/yaml.h>

//#include "majorframe.hpp"

// g++ -I/usr/local/include -L/usr/local/lib yaml_demo.cpp -lyaml-cpp

using namespace std;

int main()
{
    YAML::Node config = YAML::LoadFile("../src/config.yaml");

    for(YAML::const_iterator it = config["partitions"].begin();
        it != config["partitions"].end(); ++it)
    {
        cout<< (*it)["name"].as<string>() <<endl;
        // create partition
        for(YAML::const_iterator jt = (*it)["processes"].begin();
            jt != (*it)["processes"].end(); ++jt)
        {
            cout<< (*jt)["cmd"].as<string>() <<endl;
            // add process to partition
        }
    }
    cout<<endl;

    for(YAML::const_iterator it = config["windows"].begin();
        it != config["windows"].end(); ++it)
    {
        cout<< (*it)["length"].as<int>() <<endl;
        // create window
        for(YAML::const_iterator jt = (*it)["slices"].begin();
            jt != (*it)["slices"].end(); ++jt)
        {
            if ((*jt)["sc_partition"])
                cout<< (*jt)["sc_partition"].as<string>() <<endl;
            if ((*jt)["be_partition"])
                cout<< (*jt)["be_partition"].as<string>() <<endl;
            // create slice
        }
    }


    return 0;
}
