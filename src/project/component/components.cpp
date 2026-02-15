/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "components.h"

namespace Project::Component
{
  std::array<CompInfo, TABLE.size()> TABLE_SORTED_BY_NAME{};
}

void Project::Component::init() {
  TABLE_SORTED_BY_NAME = TABLE;
  std::ranges::sort(TABLE_SORTED_BY_NAME, [](const CompInfo &a, const CompInfo &b) {
    return strcmp(a.name, b.name) < 0;
  });
}
