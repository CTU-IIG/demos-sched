#include "config_parser.hpp"



ConfigParser::ConfigParser(std::string config_file)
    : config( YAML::LoadFile(config_file) )
{

}

using namespace std;

void ConfigParser::parse(ev::loop_ref loop, std::shared_ptr<MajorFrame> mf_ptr)
{
    for(YAML::const_iterator it = config["partitions"].begin();
        it != config["partitions"].end(); ++it)
    {
        cout<< (*it)["name"].as<string>() <<endl;
        // create partition
        partitions.emplace_back(loop,(*it)["name"].as<std::string>());

        for(YAML::const_iterator jt = (*it)["processes"].begin();
            jt != (*it)["processes"].end(); ++jt)
        {
            cout<< (*jt)["cmd"].as<string>() <<endl;
            // add process to partition
            // TODO break down cmd to array of c_str
            partitions.end()->add_process(loop, std::vector<std::string>
                {"src/infinite_proc","200000","foo"}, 1s);
        }
    }
    cout<<endl;

    for(YAML::const_iterator it = config["windows"].begin();
        it != config["windows"].end(); ++it)
    {
        cout<< (*it)["length"].as<int>() <<endl;
        // create window
        Slices slices; // use shared_ptr?

        for(YAML::const_iterator jt = (*it)["slices"].begin();
            jt != (*it)["slices"].end(); ++jt)
        {
            // TODO find and add partitions according to their names
            // slices.emplace_back
            if ((*jt)["sc_partition"])
                cout<< (*jt)["sc_partition"].as<string>() <<endl;
            if ((*jt)["be_partition"])
                cout<< (*jt)["be_partition"].as<string>() <<endl;
            // create slice
        }
        windows.push_back( Window( loop, slices,
                      std::chrono::milliseconds((*it)["length"].as<int>())) );
    }

    std::make_shared<MajorFrame>(loop, windows);

}
