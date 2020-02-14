#include "config_parser.hpp"


ConfigParser::ConfigParser(std::string config_file)
    : config( YAML::LoadFile(config_file) )
    , empty_part(freezer_path, cpuset_path, unified_path, "empty_part")
{

}

using namespace std;

void ConfigParser::parse(ev::loop_ref loop, std::shared_ptr<MajorFrame> mf_ptr)
{   
    for(YAML::const_iterator it = config["partitions"].begin();
        it != config["partitions"].end(); ++it)
    {
        cerr<< (*it)["name"].as<string>() <<endl;
        // create partition

        //partition_names.push_back( (*it)["name"].as<string>() );
        //cout<<"name "<<*partition_names.end()<<endl;
        partitions.emplace_back( freezer_path, cpuset_path, unified_path, (*it)["name"].as<string>() );
        //std::cerr<<"name "<<partitions.end()->get_name()<<std::endl;

        for(YAML::const_iterator jt = (*it)["processes"].begin();
            jt != (*it)["processes"].end(); ++jt)
        {
            cerr<< (*jt)["cmd"].as<string>() <<endl;
            // add process to partition
            partitions.back().add_process(loop, (*jt)["cmd"].as<string>(),
                                chrono::milliseconds((*jt)["budget"].as<int>()) );
        }
    }
    cerr<<endl;

    for(YAML::const_iterator it = config["windows"].begin();
        it != config["windows"].end(); ++it)
    {
        cerr<< (*it)["length"].as<int>() <<endl;
        // create window
        Slices slices; // use shared_ptr?

        for(YAML::const_iterator jt = (*it)["slices"].begin();
            jt != (*it)["slices"].end(); ++jt)
        {
            Partition *sc_part_ptr = &empty_part;
            Partition *be_part_ptr = &empty_part;
            // TODO find and add partitions according to their names
            // slices.emplace_back
            if ((*jt)["sc_partition"]){
                cerr<< (*jt)["sc_partition"].as<string>() <<endl;
                for(auto iter = partitions.begin(); iter != partitions.end(); iter++){
                    if( iter->get_name() == (*jt)["sc_partition"].as<string>() ){
                        sc_part_ptr = &(*iter);
                        break;
                    }
                }
            }
            if ((*jt)["be_partition"]){
                cerr<< (*jt)["be_partition"].as<string>() <<endl;
                for(auto iter = partitions.begin(); iter != partitions.end(); iter++){
                    if( iter->get_name() == (*jt)["be_partition"].as<string>() ){
                        be_part_ptr = &(*iter);
                        break;
                    }
                }
            }
            // create slice
            slices.emplace_back( loop, start_time, *sc_part_ptr, *be_part_ptr, (*jt)["cpu"].as<string>());
        }
        windows.emplace_back( slices, std::chrono::milliseconds((*it)["length"].as<int>()) );
    }

   mf_ptr = make_shared<MajorFrame>(loop, start_time, windows);

}
