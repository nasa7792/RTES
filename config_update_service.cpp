#include "config_update_service.hpp"

void load_config(const std::string& filename)
{
  std::ifstream file(filename);//input file stream
  nlohmann::json json_instance;
  
  file>>json_instance;
  
  auto colour=json_instance.at("colour");
  auto l1 = colour.at("lower1");
  auto u1 = colour.at("upper1");
  auto l2 = colour.at("lower2");
  auto u2 = colour.at("upper2");
  
  HSVConfig new_config;
  new_config.lower1 = cv::Scalar(l1[0], l1[1], l1[2]);
        new_config.upper1 = cv::Scalar(u1[0], u1[1], u1[2]);
        new_config.lower2 = cv::Scalar(l2[0], l2[1], l2[2]);
        new_config.upper2 = cv::Scalar(u2[0], u2[1], u2[2]);
     syslog(LOG_INFO,"loading new config");     
   std::lock_guard<std::mutex> lock(config_mutex);
   config = new_config;

  
}

void config_update_service()
{ 	//keep  a track of the last modified time
	static std::time_t last_mod_time = 0;
    struct stat file_stat;
     if (stat(CONFIG_FILE, &file_stat) == 0) {
        if (file_stat.st_mtime != last_mod_time) {
            last_mod_time = file_stat.st_mtime;
            load_config(CONFIG_FILE);
        }
}
}
