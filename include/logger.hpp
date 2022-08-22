#ifndef BOOSTMINER_LOGGER
#define BOOSTMINER_LOGGER

#include <boost/date_time.hpp>
#include <nlohmann/json.hpp>

using nlohmann::json;

namespace logger {

  void log(std::string event, json j);

}

#endif 
