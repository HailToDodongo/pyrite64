/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "updater.h"

#include "network.h"
#include "string.h"
#include <SDL3/SDL.h>

std::string Utils::Updater::getNewerVersion()
{
  auto ghReleases = Net::httpGetJson(
    "https://api.github.com/repos/" PYRITE_GH_USER "/" PYRITE_GH_REPO "/releases/latest",
    {{"Accept", "application/vnd.github+json"}}
  );

  if(!ghReleases.is_object())return "";

  auto selfVer = std::string{PYRITE_VERSION};
  auto latestVer = ghReleases.value<std::string>("tag_name", std::string{});

  int verCompare = compareSemVer(selfVer, latestVer);
  printf("Version: %s vs %s: %d\n", selfVer.c_str(), latestVer.c_str(), verCompare);

  return (verCompare < 0) ? latestVer : "";
  //return latestVer; // TEST
}

void Utils::Updater::doUpdate(const std::string &targetVersion)
{
  std::string releaseUrl = "https://github.com/" PYRITE_GH_USER "/" PYRITE_GH_REPO "/releases/tag/";
  releaseUrl += targetVersion;
  SDL_OpenURL(releaseUrl.c_str());
}
