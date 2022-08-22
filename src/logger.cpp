#include <logger.hpp>

namespace logger {

  void log(std::string event, json j) {

    boost::posix_time::ptime timestamp { boost::posix_time::microsec_clock::universal_time() };

    j["timestamp"] = to_iso_extended_string(timestamp);
    j["event"] = event;

    std::cout << j.dump() << std::endl;

  }

}

