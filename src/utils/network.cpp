/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "network.h"

#include "logger.h"
#include "proc.h"

nlohmann::json Utils::Net::httpGetJson(
  const std::string &url,
  const std::unordered_map<std::string, std::string> &headers
) {

  std::string cmd = "curl --no-progress-meter "
        "--connect-timeout 2 "
        "--max-time 2 "
        "-L \"" + url + "\"";
  for (const auto &[key, value] : headers) {
    cmd += " -H \"" + key + ": " + value + "\"";
  }

  std::string response = Proc::runSync(cmd);
  if (response.empty())
  {
    Logger::log("HTTP GET request failed or returned empty response");
    return {};
  }

  try {
    return nlohmann::json::parse(response);
  } catch (std::exception &e) {
    Logger::log(std::string("Failed to parse JSON response: ") + e.what());
    return {};
  }
}
