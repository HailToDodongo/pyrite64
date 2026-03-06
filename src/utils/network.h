/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include <unordered_map>
#include "json.hpp"

namespace Utils::Net
{
  nlohmann::json httpGetJson(
    const std::string &url,
    const std::unordered_map<std::string, std::string> &headers = {}
  );
}
