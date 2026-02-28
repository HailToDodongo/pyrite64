/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "time.h"

#include <chrono>
#include <ctime>
#include <iomanip>

std::string Utils::Time::getDateTimeStr()
{
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);

  std::stringstream ss;
  ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");
  return ss.str();
}
