/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"

#include <filesystem>
#include <thread>
#include "../utils/fs.h"
#include "../utils/logger.h"
#include "../utils/proc.h"
#include "../utils/string.h"
#include "../utils/textureFormats.h"

namespace fs = std::filesystem;
using AT = Project::FileType;

namespace
{
  struct AssetBuilder
  {
    Build::BuildFunc func;
    const char* name;
  };

  constexpr auto assetBuilders = std::to_array<AssetBuilder>({
    {Build::buildT3DMAssets,    "3D Model"},
    {Build::buildFontAssets,    "Font"},
    {Build::buildTextureAssets, "Texture"},
    {Build::buildAudioAssets,   "Audio"},
    {Build::buildPrefabAssets,  "Prefab"},
  });
}

void Build::SceneCtx::addAsset(const Project::AssetManagerEntry &entry)
{
  assetUUIDToIdx[entry.getUUID()] = assetList.size();
  if(entry.romPath.size() > 5) {
    auto outNameNoPrefix = entry.romPath.substr(5); // remove "rom:/"
    assetFileMap += "if(path == \"" + outNameNoPrefix + "\")return " + std::to_string(assetList.size()) + ";\n";
  }

  uint32_t flags = 0;
  if(entry.type == AT::FONT) {
    flags |= 0x01; // KEEP_LOADED
  }

  assetList.push_back({entry.romPath, stringOffset, (uint32_t)entry.type, flags});
  stringOffset += entry.romPath.size() + 1;
}

bool Build::buildProject(const std::string &path)
{
  Project::Project project{path};
  Utils::Logger::log("Building project...");

  if(project.conf.pathN64Inst.empty())
  {
    // read env
  #if defined(_WIN32)
    /*char* n64InstEnv = nullptr;
    size_t envSize = 0;
    if(_dupenv_s(&n64InstEnv, &envSize, "N64_INST") == 0 && n64InstEnv != nullptr) {
      project.conf.pathN64Inst = n64InstEnv;
      free(n64InstEnv);
    }*/
   project.conf.pathN64Inst = "/pyrite64-sdk";
  #else
    char* n64InstEnv = getenv("N64_INST");
    if(n64InstEnv != nullptr) {
      project.conf.pathN64Inst = n64InstEnv;
    }
  #endif
  } else {
  #if defined(_WIN32)
    //_putenv_s("N64_INST", project.conf.pathN64Inst.c_str());
  #else
    setenv("N64_INST", project.conf.pathN64Inst.c_str(), 0);
  #endif
  }

  #if defined(_WIN32)
    _putenv_s("N64_INST", project.conf.pathN64Inst.c_str());
    _putenv_s("MSYSTEM", "MINGW64");
    _putenv_s("PATH", "C:\\msys64\\mingw64\\bin;C:\\msys64\\usr\\bin");
  #endif

  auto fsDataPath = fs::absolute(fs::path{path} / "filesystem" / "p64");
  if (!fs::exists(fsDataPath)) {
    fs::create_directories(fsDataPath);
  }

  SceneCtx sceneCtx{};
  sceneCtx.toolchain.scan();
  sceneCtx.project = &project;

  // Global project config
  sceneCtx.files.push_back("filesystem/p64/conf");

  // Asset-Manager
  for (auto &typed : project.getAssets().getEntries()) {
    for (auto &entry : typed) {
      if (entry.conf.exclude || entry.type == Project::FileType::UNKNOWN) continue;
      sceneCtx.addAsset(entry);
    }
  }

  // User scripts
  auto userDirs = Utils::FS::scanDirs(path + "/src/user");
  std::string userCodeRules = "";
  for (const auto &dir : userDirs) {
    userCodeRules += "src += $(wildcard src/user/" + dir + "/*.cpp)\n";
  }

  if(!buildNodeGraphAssets(project, sceneCtx)) {
    Utils::Logger::log(std::string("Graph-Asset build failed!", Utils::Logger::LEVEL_ERROR));
    return false;
  }

  // Scripts
  buildGlobalScripts(project, sceneCtx);
  buildScripts(project, sceneCtx);

  // Scenes
  project.getScenes().reload();
  const auto &scenes = project.getScenes().getEntries();

  std::string sceneMapStr{};
  std::string sceneNameStr{};
  for (const auto &scene : scenes) {
    sceneMapStr += "if(path == \"" + scene.name + "\")return " + std::to_string(scene.id) + ";\n";
    sceneNameStr += "\"" + scene.name + "\",\n";
    try
    {
      buildScene(project, scene, sceneCtx);
    } catch(const std::exception &e)
    {
      Utils::Logger::log(std::string("Scene build failed: ") + e.what(), Utils::Logger::LEVEL_ERROR);
      return false;
    }
  }

  auto sceneTableHeader = Utils::replaceAll(Utils::FS::loadTextFile("data/scripts/sceneTable.h"), {
    {"{{SCENE_MAP}}", sceneMapStr},
    {"{{SCENE_COUNT}}", std::to_string(scenes.size())}
  });
  Utils::FS::saveTextFile(project.getPath() + "/src/p64/sceneTable.h", sceneTableHeader);

  Utils::FS::saveTextFile(project.getPath() + "/src/p64/sceneTable.cpp",
    "#include \"sceneTable.h\"\n"
    "\n"
    "namespace P64::SceneManager {\n"
    "  const char* SCENE_NAMES["+std::to_string(scenes.size())+"] = {\n" + sceneNameStr + "};\n"
    "}\n"
  );


  for(auto &builder : assetBuilders)
  {
    if(!builder.func(project, sceneCtx)) {
      Utils::Logger::log(std::string(builder.name) + " Asset build failed!", Utils::Logger::LEVEL_ERROR);
      return false;
    }
  }

  auto assetTableCode = Utils::replaceAll(
    Utils::FS::loadTextFile("data/scripts/assetTable.h"),
    "{{ASSET_MAP}}", sceneCtx.assetFileMap
  );
  Utils::FS::saveTextFile(project.getPath() + "/src/p64/assetTable.h", assetTableCode);

  // Asset table
  Utils::BinaryFile fileList{};
  fileList.write<uint32_t>(sceneCtx.assetList.size());
  uint32_t baseOffset = (sceneCtx.assetList.size() * sizeof(uint32_t)*2) + sizeof(uint32_t);
  for (auto &entry : sceneCtx.assetList) {
    fileList.write(baseOffset + entry.stringOffset);
    uint32_t ptr = entry.type << (32-4);
    ptr |= entry.flags << (32-8);
    fileList.write(ptr);
  }
  for (auto &entry : sceneCtx.assetList) {
    fileList.writeChars(entry.path.c_str(), entry.path.size()+1);
  }
  fileList.writeToFile(fsDataPath / "a");

  // kep order stable to detect makefile changes
  auto filesSorted = sceneCtx.files;
  std::sort(filesSorted.begin(), filesSorted.end());

  // Makefile
  auto makefile = Utils::replaceAll(
    Utils::FS::loadTextFile("data/build/baseMakefile.mk"),
    {
      {"{{N64_INST}}",          project.conf.pathN64Inst},
      {"{{ROM_NAME}}",          project.conf.romName},
      {"{{PROJECT_NAME}}",      project.conf.name},
      {"{{ASSET_LIST}}",        Utils::join(filesSorted, " ")},
      {"{{USER_CODE_DIRS}}",    userCodeRules},
      {"{{P64_SELF_PATH}}",     Utils::Proc::getSelfPath()},
      {"{{PROJECT_SELF_PATH}}", fs::absolute(path).string()},
    }
  );

  auto mkPath = fs::absolute(path) / "Makefile";
  auto oldMakefile = Utils::FS::loadTextFile(mkPath);
  if (makefile != oldMakefile) {
    Utils::Logger::log("Makefile changed, clean build");
    Utils::FS::saveTextFile(mkPath, makefile);
    sceneCtx.toolchain.runCmdSyncLogged("make -C \"" + path + "\" cleanCode");
  }

  {
    Utils::BinaryFile f{};
    f.write<uint32_t>(project.conf.sceneIdOnBoot);
    f.write<uint32_t>(project.conf.sceneIdOnReset);
    for(uint32_t i=0; i<sceneCtx.autoLoadFontUUIDs.size(); ++i) {
      auto uuid = sceneCtx.autoLoadFontUUIDs[i];
      f.write<uint16_t>(uuid == 0 ? 0xFFFF : sceneCtx.assetUUIDToIdx[uuid]);
    }
    f.writeToFile(fsDataPath / "conf");
  }

  // Build
  bool success = sceneCtx.toolchain.runCmdSyncLogged("make -C \"" + path + "\" -j8");

  if(success) {
    Utils::Logger::log("Build done!");
  } else {
    Utils::Logger::log("Build failed!", Utils::Logger::LEVEL_ERROR);
  }
  return success;
}


bool Build::assetBuildNeeded(const Project::AssetManagerEntry &asset, const fs::path &outPath)
{
  auto ageSrc = Utils::FS::getFileAge(asset.path);
  auto ageDst = Utils::FS::getFileAge(outPath);
  if(ageSrc < ageDst) {
    //Utils::Logger::log("Skipping Asset (up to date): " + asset.outPath);
    return false;
  }
  Utils::Logger::log("Building Asset: " + asset.path);
  return true;
}
