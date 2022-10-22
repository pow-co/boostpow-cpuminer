#include <logger.hpp>
#include <mutex>

namespace logger {
  std::mutex Mutex;

  void log(std::string event, json j) {
    std::lock_guard<std::mutex> lock(Mutex);

    boost::posix_time::ptime timestamp { boost::posix_time::microsec_clock::universal_time() };

    std::cout << json{{"event", event}, {"timestmap", to_iso_extended_string(timestamp)}, {"message", j} }.dump() << std::endl;

  }

}

