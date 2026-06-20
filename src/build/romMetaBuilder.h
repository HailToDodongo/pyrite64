/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>
#include "../project/project.h"

namespace Build
{
  // Builds the target-specific N64_ROM_* Makefile lines for the ED / homebrew ROM header
  // (category, region, savetype, rtc, controllers, ...) and, when metadata is enabled,
  // (re)generates <project>/metadata/metadata.ini plus its referenced text/image files.
  // Returns the Makefile snippet to substitute for {{ROM_HEADER_FLAGS}}.
  std::string buildRomHeaderFlags(Project::Project &project);
}
