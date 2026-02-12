/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assetManager.h"
#include "../context.h"
#include <filesystem>
#include <format>
#include <chrono>
#include <unordered_set>

#include "SHA256.h"
#include "../utils/codeParser.h"
#include "../utils/fs.h"
#include "../utils/hash.h"
#include "../utils/json.h"
#include "../utils/jsonBuilder.h"
#include "../utils/logger.h"
#include "../utils/meshGen.h"
#include "../utils/string.h"
#include "../utils/textureFormats.h"
#include "tiny3d/tools/gltf_importer/src/parser.h"

namespace fs = std::filesystem;

namespace
{
  fs::path getCodePath(Project::Project *project) {
    auto res = fs::path{project->getPath()} / "src" / "user";
    if (!fs::exists(res)) {
      fs::create_directory(res);
    }
    return res;
  }

  fs::path getAssetPath(Project::Project *project) {
    auto res = fs::path{project->getPath()} / "assets";
    if (!fs::exists(res)) {
      fs::create_directory(res);
    }
    return res;
  }

  std::string getAssetROMPath(const std::string &path, const std::string &basePath)
  {
    auto pathAbs = Utils::FS::toUnixPath(fs::absolute(path));
    pathAbs = pathAbs.substr(basePath.length());
    pathAbs = Utils::replaceFirst(pathAbs, "/assets/", "filesystem/");
    return pathAbs;
  }

  std::string changeExt(const std::string &path, const std::string &newExt)
  {
    auto p = fs::path(path);
    p.replace_extension(newExt);
    return p.string();
  }


  void deserialize(Project::AssetConf &conf, const fs::path &pathMeta)
  {
    auto doc = Utils::JSON::loadFile(pathMeta);
    if (doc.is_object()) {
      conf.uuid = doc.value<uint64_t>("uuid", 0);
      conf.format = doc["format"];
      conf.baseScale = doc["baseScale"];
      conf.compression = (Project::ComprTypes)doc.value<int>("compression", 0);
      conf.gltfBVH = doc["gltfBVH"];
      Utils::JSON::readProp(doc, conf.gltfCollision);
      Utils::JSON::readProp(doc, conf.wavForceMono);
      Utils::JSON::readProp(doc, conf.wavResampleRate);
      Utils::JSON::readProp(doc, conf.wavCompression);
      Utils::JSON::readProp(doc, conf.fontId);
      Utils::JSON::readProp(doc, conf.fontCharset);

      conf.exclude = doc["exclude"];
    }
  }
}

std::string Project::AssetConf::serialize() const {
  return Utils::JSON::Builder{}
    .set("uuid", uuid)
    .set("format", format)
    .set("baseScale", baseScale)
    .set("compression", static_cast<int>(compression))
    .set("gltfBVH", gltfBVH)
    .set(gltfCollision)
    .set(wavForceMono)
    .set(wavResampleRate)
    .set(wavCompression)
    .set(fontId)
    .set(fontCharset)
    .set("exclude", exclude)
    .toString();
}

Project::AssetManager::AssetManager(Project* pr)
  : project{pr}
{
  defaultScript = Utils::FS::loadTextFile("data/scripts/default.cpp");
}

Project::AssetManager::~AssetManager() {

}

void Project::AssetManager::reloadEntry(AssetManagerEntry &entry, const std::string &path)
{
  switch(entry.type)
  {
    case FileType::IMAGE:
    {
      bool isMono = Utils::isTexFormatMono(static_cast<Utils::TexFormat>(entry.conf.format));
      entry.texture = std::make_shared<Renderer::Texture>(ctx.gpu, path, isMono);
    } break;

    case FileType::PREFAB:
    {
      entry.prefab = std::make_shared<Prefab>();
      entry.prefab->deserialize(Utils::FS::loadTextFile(path));
    } break;

    case FileType::MODEL_3D:
    {
      try{
        T3DM::config = {
          .globalScale = (float)entry.conf.baseScale,
          .animSampleRate = 60,
          //.ignoreMaterials = args.checkArg("--ignore-materials"),
          //.ignoreTransforms = args.checkArg("--ignore-transforms"),
          .createBVH = entry.conf.gltfBVH,
          .verbose = false,
          .assetPath = "assets/",
          .assetPathFull = fs::absolute(project->getPath() + "/assets").string(),
        };

        entry.t3dmData = T3DM::parseGLTF(path.c_str());
        if (!entry.t3dmData.models.empty()) {
          if (!entry.mesh3D) {
            entry.mesh3D = std::make_shared<Renderer::N64Mesh>();
          }
          entry.mesh3D->fromT3DM(entry.t3dmData, *this);
        }
      } catch (std::exception &e) {
        Utils::Logger::log("Failed to load 3D model asset: " + entry.path + " - " + e.what(), Utils::Logger::LEVEL_ERROR);
      }
    }
    break;

    default: break;
  }
}

void Project::AssetManager::reload() {
  for (auto &e : entries)e.clear();
  entriesMap.clear();
  watchFiles.clear();
  watchInitialized = false;

  auto assetPath = fs::path{project->getPath()} / "assets";
  if (!fs::exists(assetPath)) {
    fs::create_directory(assetPath);
  }

  auto projectBase = fs::absolute(project->getPath()).string();

  // scan all files
  for (const auto &entry : fs::recursive_directory_iterator{assetPath}) {
    if (entry.is_regular_file()) {
      auto path = entry.path();
      auto ext = path.extension().string();
      watchFiles[path.string()] = Utils::FS::getFileAge(path);

      std::string outPath{};
      outPath = getAssetROMPath(path.string(), projectBase);

      FileType type = FileType::UNKNOWN;
      if (ext == ".png") {
        type = FileType::IMAGE;
        outPath = changeExt(outPath, ".sprite");
      } else if (ext == ".wav" || ext == ".mp3") {
        type = FileType::AUDIO;
        outPath = changeExt(outPath, ".wav64");
      } else if (ext == ".glb" || ext == ".gltf") {
        type = FileType::MODEL_3D;
        outPath = changeExt(outPath, ".t3dm");
      } else if (ext == ".ttf") {
        type = FileType::FONT;
        outPath = changeExt(outPath, ".font64");
      } else if (ext == ".prefab") {
        type = FileType::PREFAB;
        outPath = changeExt(outPath, ".pf");
      } else if (ext == ".p64graph") {
        type = FileType::NODE_GRAPH;
        outPath = changeExt(outPath, ".pg");
      }

      auto romPath = outPath;
      romPath.replace(0, std::string{"filesystem/"}.length(), "rom:/");

      AssetManagerEntry entry{
        .name = path.filename().string(),
        .path = path.string(),
        .outPath = outPath,
        .romPath = romPath,
        .type = type,
      };

      entry.conf.baseScale = 16;

      // check if meta-data exists
      auto pathMeta = path;
      pathMeta += ".conf";
      if (type != FileType::UNKNOWN && fs::exists(pathMeta)) {
        deserialize(entry.conf, pathMeta);
      }

      if(entry.conf.uuid == 0) {
        entry.conf.uuid = Utils::Hash::randomU64();
      }

      if (type == FileType::IMAGE)
      {
        if(entry.path.ends_with(".bci.png")) {
          entry.conf.format = (int)Utils::TexFormat::BCI_256;
          outPath = changeExt(outPath, ".bci");
        }
        if(ctx.window)reloadEntry(entry, path.string());
      }

      if( type == FileType::FONT && entry.conf.fontCharset.value.empty())
      {
        entry.conf.fontCharset.value =
          " !\"#$%&\'()*+,-./"                 "\n"
          "0123456789:;<=>?@"                  "\n"
          "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"  "\n"
          "abcdefghijklmnopqrstuvwxyz{|}~";
      }

      if (type == FileType::PREFAB) {
        reloadEntry(entry, path.string());
        entry.conf.uuid = entry.prefab->uuid.value;
      }

      entries[(int)type].push_back(entry);
    }
  }

  // now load models (after all textures are there now)
  for (auto &typed : entries) {
    for (auto &entry : typed) {
      if (entry.type == FileType::MODEL_3D) {
        reloadEntry(entry, entry.path);
      }
    }
  }

  auto codePath = getCodePath(project);
  for (const auto &entry : fs::recursive_directory_iterator{codePath}) {
    if (entry.is_regular_file()) {
      auto path = entry.path();
      auto ext = path.extension().string();

      FileType type = FileType::UNKNOWN;
      if (ext == ".cpp") {
        type = FileType::CODE_OBJ;
      } else {
        continue;
      }

      watchFiles[path.string()] = Utils::FS::getFileAge(path);

      auto code = Utils::FS::loadTextFile(path);

      auto uuidPos = code.find("::Script::");
      if (uuidPos == std::string::npos)
      {
        type = FileType::CODE_GLOBAL;
        uuidPos = code.find("::GlobalScript::");
        if (uuidPos == std::string::npos)continue;
        uuidPos += 16;
      } else
      {
        uuidPos += 10;
      }

      if (uuidPos + 16 > code.size())continue;
      auto uuidStr = code.substr(uuidPos, 16);
      uint64_t uuid = 0;
      try {
        uuid = std::stoull(uuidStr, nullptr, 16);
      } catch (...) {
        continue;
      }

      AssetManagerEntry entry{
        .name = path.filename().string(),
        .path = path.string(),
        .type = type,
        .params = Utils::CPP::parseDataStruct(code, "Data")
      };
      entry.conf.uuid = uuid;

      entries[(int)type].push_back(entry);
    }
  }

  // sort by name
  for (auto &typed : entries) {
    std::sort(typed.begin(), typed.end(), [](const AssetManagerEntry &a, const AssetManagerEntry &b) {
      return a.name < b.name;
    });
  }

  for (auto &typed : entries)
  {
    int idx = 0;
    for (auto &entry : typed)
    {
      entriesMap[entry.getUUID()] = {(int)entry.type, idx};
      ++idx;
    }
  }
}

bool Project::AssetManager::pollWatch()
{
  using Clock = std::chrono::steady_clock;
  constexpr auto kMinInterval = std::chrono::milliseconds(1000);

  auto now = Clock::now();
  if (watchInitialized && (now - watchLastCheck) < kMinInterval) {
    return false;
  }
  watchInitialized = true;
  watchLastCheck = now;

  std::unordered_set<std::string> seen{};
  bool changed = false;

  auto assetPath = fs::path{project->getPath()} / "assets";
  if (fs::exists(assetPath)) {
    for (const auto &entry : fs::recursive_directory_iterator{assetPath}) {
      if (!entry.is_regular_file()) continue;
      auto path = entry.path();
      auto ext = path.extension().string();

      auto pathStr = path.string();
      seen.insert(pathStr);
      uint64_t age = Utils::FS::getFileAge(path);

      auto it = watchFiles.find(pathStr);
      if (it == watchFiles.end() || it->second != age) {
        changed = true;
        break;
      }
    }
  }

  auto codePath = getCodePath(project);
  if (!changed && fs::exists(codePath)) {
    for (const auto &entry : fs::recursive_directory_iterator{codePath}) {
      if (!entry.is_regular_file()) continue;
      auto path = entry.path();
      if (path.extension().string() != ".cpp") continue;

      auto pathStr = path.string();
      seen.insert(pathStr);
      uint64_t age = Utils::FS::getFileAge(path);

      auto it = watchFiles.find(pathStr);
      if (it == watchFiles.end() || it->second != age) {
        changed = true;
        break;
      }
    }
  }

  if (!changed) {
    for (const auto &pair : watchFiles) {
      if (seen.find(pair.first) == seen.end()) {
        changed = true;
        break;
      }
    }
  }

  if (changed) {
    reload();
  }

  return changed;
}

void Project::AssetManager::reloadAssetByUUID(uint64_t uuid) {
  auto asset = getEntryByUUID(uuid);
  if (!asset)return;
  reloadEntry(*asset, asset->path);
}

const std::shared_ptr<Renderer::Texture> & Project::AssetManager::getFallbackTexture()
{
  if(!fallbackTex) {
    fallbackTex = std::make_shared<Renderer::Texture>(ctx.gpu, "data/img/fallback.png");
  }
  return fallbackTex;
}

void Project::AssetManager::save()
{
  for(auto &typed : entries) {
    for(auto &entry : typed)
    {
      if(entry.type == FileType::UNKNOWN || entry.type == FileType::CODE_OBJ || entry.type == FileType::CODE_GLOBAL) {
        continue;
      }

      auto pathMeta = entry.path + ".conf";
      auto json = entry.conf.serialize();

      auto oldFile = Utils::FS::loadTextFile(pathMeta);

      // if the meta-data changed, force a recompile of the asset by deleting the target
      if (oldFile == json)continue;
      Utils::Logger::log("Asset meta-data changed, forcing recompile: " + entry.outPath, Utils::Logger::LEVEL_INFO);
      fs::remove(project->getPath() + "/" + entry.outPath);
      Utils::FS::saveTextFile(pathMeta, entry.conf.serialize());
    }
  }
}

void Project::AssetManager::createScript(const std::string &name, const std::string &subDir) {
  auto codePath = getCodePath(project);
  fs::path dirPath = codePath;

  if (!subDir.empty()) {
    fs::path relPath{subDir};
    if (!relPath.is_absolute()) {
      relPath = relPath.lexically_normal();
      bool hasParent = false;
      for (const auto &part : relPath) {
        if (part == "..") {
          hasParent = true;
          break;
        }
      }
      if (!hasParent) {
        dirPath /= relPath;
      }
    }
  }

  fs::create_directories(dirPath);

  auto filePath = dirPath / (name + ".cpp");

  uint64_t uuid = Utils::Hash::sha256_64bit("CODE:" + filePath.string() + std::to_string(rand()));
  auto uuidStr = std::format("{:016X}", uuid);
  uuidStr[0] = 'C'; // avoid leading numbers since it's used as a namespace name

  if (fs::exists(filePath))return;

  auto code = defaultScript;
  code = Utils::replaceAll(code, "__UUID__", uuidStr);

  Utils::FS::saveTextFile(filePath, code);
  reload();
}

uint64_t Project::AssetManager::createNodeGraph(const std::string &name)
{
  auto assetPath = getAssetPath(project);
  auto filePath = assetPath / (name + ".p64graph");

  if (fs::exists(filePath))return 0;

  Utils::FS::saveTextFile(filePath, "{\"nodes\": [], \"links\": []}");
  reload();

  auto entry = getByName(name);
  return entry ? entry->getUUID() : 0;
}

Project::AssetManagerEntry *Project::AssetManager::getByPath(const std::string &path)
{
  fs::path pathIn{path};
  for (auto &typed : entries) {
    for (auto &entry : typed) {
      if (fs::path{entry.path} == pathIn) {
        return &entry;
      }
    }
  }
  return nullptr;
}