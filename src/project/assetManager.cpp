/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "assetManager.h"
#include "../context.h"
#include <filesystem>

#include "SHA256.h"
#include "../utils/hash.h"

void Project::AssetManager::reload() {
  entries.clear();

  auto assetPath = std::filesystem::path{ctx.project->getPath()} / "assets";
  if (!std::filesystem::exists(assetPath)) {
    std::filesystem::create_directory(assetPath);
  }

  // scan all files
  for (const auto &entry : std::filesystem::directory_iterator{assetPath}) {
    if (entry.is_regular_file()) {
      auto path = entry.path();
      auto ext = path.extension().string();

      FileType type = FileType::UNKNOWN;
      if (ext == ".png") {
        type = FileType::IMAGE;
      } else if (ext == ".wav" || ext == ".mp3") {
        type = FileType::AUDIO;
      } else if (ext == ".glb" || ext == ".gltf") {
        type = FileType::MODEL_3D;
      }

      Renderer::Texture *texture{nullptr};
      if (type == FileType::IMAGE) {
        texture = new Renderer::Texture{ctx.gpu, path.string()};
      }

      uint64_t uuid = Utils::Hash::sha256_64bit("ASSET:" + path.string());
      entries.push_back({
        .uuid = uuid,
        .name = path.filename().string(),
        .path = path.string(),
        .type = type,
        .texture = texture
      });
    }
  }
}
