/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assetManager.h"
#include "../context.h"
#include <filesystem>
#include <format>

#include "SHA256.h"
#include "../utils/codeParser.h"
#include "../utils/fs.h"
#include "../utils/hash.h"
#include "../utils/json.h"
#include "../utils/jsonBuilder.h"
#include "../utils/logger.h"
#include "../utils/meshGen.h"
#include "../utils/string.h"
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

  std::string getAssetROMPath(const std::string &path, const std::string &basePath)
  {
    auto pathAbs = fs::absolute(path).string();
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
}

std::string Project::AssetManager::AssetConf::serialize() const {
  Utils::JSON::Builder builder{};
  builder.set("format", format);
  builder.set("baseScale", baseScale);
  builder.set("compression", static_cast<int>(compression));
  builder.set("gltfBVH", gltfBVH);
  builder.set("exclude", exclude);
  return builder.toString();
}

Project::AssetManager::AssetManager(Project* pr)
  : project{pr}, fallbackTex{ctx.gpu, "data/img/fallback.png"}
{
  defaultScript = Utils::FS::loadTextFile("data/scripts/default.cpp");
}

Project::AssetManager::~AssetManager() {

}

void Project::AssetManager::reloadEntry(Entry &entry, const std::string &path)
{
  switch(entry.type)
  {
    case FileType::IMAGE:
    {
      entry.texture = std::make_shared<Renderer::Texture>(ctx.gpu, path);
      break;
    }

    case FileType::MODEL_3D:
    {
      try {
        entry.t3dmData = parseGLTF(path.c_str(), entry.conf.baseScale);
        if (!entry.t3dmData.models.empty()) {
          if (!entry.mesh3D) {
            entry.mesh3D = std::make_shared<Renderer::N64Mesh>();
          }
          entry.mesh3D->fromT3DM(entry.t3dmData, *this);
        }
      } catch (...) {
        Utils::Logger::log("Failed to load 3D model asset: " + entry.path);
      }
    }
    break;

    default: break;
  }
}

void Project::AssetManager::reload() {
  for (auto &e : entries)e.clear();

  auto assetPath = fs::path{project->getPath()} / "assets";
  if (!fs::exists(assetPath)) {
    fs::create_directory(assetPath);
  }

  auto projectBase = fs::absolute(project->getPath()).string();

  // scan all files
  for (const auto &entry : fs::directory_iterator{assetPath}) {
    if (entry.is_regular_file()) {
      auto path = entry.path();
      auto ext = path.extension().string();

      std::string outPath{};
      outPath = getAssetROMPath(path, projectBase);

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
      }

      auto romPath = outPath;
      romPath.replace(0, std::string{"filesystem/"}.length(), "rom:/");

      uint64_t uuid = Utils::Hash::sha256_64bit("ASSET:" + path.string());
      Entry entry{
        .uuid = uuid,
        .name = path.filename().string(),
        .path = path.string(),
        .outPath = outPath,
        .romPath = romPath,
        .type = type,
      };

      // check if meta-data exists
      auto pathMeta = path;
      pathMeta += ".conf";
      if (type != FileType::UNKNOWN && fs::exists(pathMeta))
      {
        auto doc = Utils::JSON::loadFile(pathMeta);
        if (doc.is_object()) {
          auto &conf = entry.conf;
          conf.format = Utils::JSON::readInt(doc, "format");
          conf.baseScale = Utils::JSON::readInt(doc, "baseScale");
          conf.compression = (ComprTypes)Utils::JSON::readInt(doc, "compression");
          conf.gltfBVH = Utils::JSON::readBool(doc, "gltfBVH");
          conf.exclude = Utils::JSON::readBool(doc, "exclude");
        }
      }

      if (type == FileType::IMAGE && ctx.window) {
        reloadEntry(entry, path.string());
      }

      entries[(int)type].push_back(entry);
      entriesMap[entry.uuid] = static_cast<int>(entries.size() - 1);
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
  for (const auto &entry : fs::directory_iterator{codePath}) {
    if (entry.is_regular_file()) {
      auto path = entry.path();
      auto ext = path.extension().string();

      FileType type = FileType::UNKNOWN;
      if (ext == ".cpp") {
        type = FileType::CODE;
      } else {
        continue;
      }

      auto code = Utils::FS::loadTextFile(path);

      auto uuidPos = code.find("Script::");
      if (uuidPos == std::string::npos)continue;
      uuidPos += 8;
      if (uuidPos + 16 > code.size())continue;
      auto uuidStr = code.substr(uuidPos, 16);
      uint64_t uuid = 0;
      try {
        uuid = std::stoull(uuidStr, nullptr, 16);
      } catch (...) {
        continue;
      }

      Entry entry{
        .uuid = uuid,
        .name = path.filename().string(),
        .path = path.string(),
        .type = type,
        .params = Utils::CPP::parseDataStruct(code, "Data")
      };

      entries[(int)type].push_back(entry);
      entriesMap[entry.uuid] = static_cast<int>(entries.size() - 1);
    }
  }
}

void Project::AssetManager::reloadAssetByUUID(uint64_t uuid) {
  auto asset = getEntryByUUID(uuid);
  if (!asset)return;
  reloadEntry(*asset, asset->path);
}

void Project::AssetManager::save()
{
  for(auto &typed : entries) {
    for(auto &entry : typed)
    {
      if(entry.type == FileType::UNKNOWN || entry.type == FileType::CODE) {
        continue;
      }

      auto pathMeta = entry.path + ".conf";
      auto json = entry.conf.serialize();
      Utils::FS::saveTextFile(pathMeta, entry.conf.serialize());
    }
  }
}

void Project::AssetManager::createScript(const std::string &name) {
  auto codePath = getCodePath(project);
  auto filePath = codePath / (name + ".cpp");

  uint64_t uuid = Utils::Hash::sha256_64bit("CODE:" + filePath.string() + std::to_string(rand()));
  auto uuidStr = std::format("{:016X}", uuid);
  uuidStr[0] = 'C'; // avoid leading numbers since it's used as a namespace name

  if (fs::exists(filePath))return;

  auto code = defaultScript;
  code = Utils::replaceAll(code, "__UUID__", uuidStr);

  Utils::FS::saveTextFile(filePath, code);
  reload();
}
