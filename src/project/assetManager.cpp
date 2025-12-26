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


  void deserialize(Project::AssetManager::AssetConf &conf, const std::filesystem::path &pathMeta)
  {
    auto doc = Utils::JSON::loadFile(pathMeta);
    if (doc.is_object()) {
      conf.format = Utils::JSON::readInt(doc, "format");
      conf.baseScale = Utils::JSON::readInt(doc, "baseScale");
      conf.compression = (Project::ComprTypes)Utils::JSON::readInt(doc, "compression");
      conf.gltfBVH = Utils::JSON::readBool(doc, "gltfBVH");
      conf.exclude = Utils::JSON::readBool(doc, "exclude");
      Utils::JSON::readProp(doc, conf.wavForceMono);
      Utils::JSON::readProp(doc, conf.wavResampleRate);
      Utils::JSON::readProp(doc, conf.wavCompression);
      Utils::JSON::readProp(doc, conf.fontId);
      Utils::JSON::readProp(doc, conf.fontCharset);
    }
  }
}

std::string Project::AssetManager::AssetConf::serialize() const {
  return Utils::JSON::Builder{}
    .set("format", format)
    .set("baseScale", baseScale)
    .set("compression", static_cast<int>(compression))
    .set("gltfBVH", gltfBVH)
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

void Project::AssetManager::reloadEntry(Entry &entry, const std::string &path)
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

      uint64_t uuid = Utils::Hash::sha256_64bit("ASSET:" + path.string());

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
      }

      auto romPath = outPath;
      romPath.replace(0, std::string{"filesystem/"}.length(), "rom:/");

      Entry entry{
        .uuid = uuid,
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
        entry.uuid = entry.prefab->uuid.value;
      }

      entries[(int)type].push_back(entry);
      entriesMap[entry.uuid] = {(int)type, static_cast<int>(entries[(int)type].size() - 1) };
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
        type = FileType::CODE_OBJ;
      } else {
        continue;
      }

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

      Entry entry{
        .uuid = uuid,
        .name = path.filename().string(),
        .path = path.string(),
        .type = type,
        .params = Utils::CPP::parseDataStruct(code, "Data")
      };

      entries[(int)type].push_back(entry);
      entriesMap[entry.uuid] = {(int)type, static_cast<int>(entries[(int)type].size() - 1) };
    }
  }
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
      Utils::FS::delFile(project->getPath() + "/" + entry.outPath);
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
