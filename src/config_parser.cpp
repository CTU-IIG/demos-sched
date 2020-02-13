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

        //partition_names.push_back( (*it)["name"].as<string>() );
        //cout<<"name "<<*partition_names.end()<<endl;
        partitions.emplace_back( loop, (*it)["name"].as<string>() );
        std::cerr<<"name "<<partitions.end()->get_name()<<std::endl;

        for(YAML::const_iterator jt = (*it)["processes"].begin();
            jt != (*it)["processes"].end(); ++jt)
        {
            cout<< (*jt)["cmd"].as<string>() <<endl;
            //break down cmd
            char *saveptr, *token;
            vector<char*> cmds;
            for ( char* cmd = const_cast<char*>((*jt)["cmd"].as<string>().c_str());; cmd = nullptr) {
                token = strtok_r(cmd," ",&saveptr);
                if( token == nullptr )
                    break;
                cmds.push_back(token);
            }
            // add process to partition

            partitions.end()->add_process(loop, cmds,
                                chrono::milliseconds((*jt)["budget"].as<int>()) );
        }
    }
    cout<<endl;

//    for(YAML::const_iterator it = config["windows"].begin();
//        it != config["windows"].end(); ++it)
//    {
//        cout<< (*it)["length"].as<int>() <<endl;
//        // create window
//        Slices slices; // use shared_ptr?

//        for(YAML::const_iterator jt = (*it)["slices"].begin();
//            jt != (*it)["slices"].end(); ++jt)
//        {
//            // TODO find and add partitions according to their names
//            // slices.emplace_back
//            if ((*jt)["sc_partition"])
//                cout<< (*jt)["sc_partition"].as<string>() <<endl;
//            if ((*jt)["be_partition"])
//                cout<< (*jt)["be_partition"].as<string>() <<endl;
//            // create slice
//        }
//        windows.push_back( Window( loop, slices,
//                      std::chrono::milliseconds((*it)["length"].as<int>())) );
//    }

//    std::make_shared<MajorFrame>(loop, windows);

}
