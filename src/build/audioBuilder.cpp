/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "projectBuilder.h"
#include "../utils/string.h"
#include "../utils/fs.h"
#include "../utils/logger.h"
#include "../utils/proc.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace
{
  namespace WavCompr = Project::WavCompression;

  std::string outStem(const fs::path &outPath)
  {
    auto p = outPath;
    p.replace_extension("");
    return p.string();
  }
}

bool Build::buildAudioAssets(Project::Project &project, SceneCtx &sceneCtx)
{
  fs::path binDir = fs::path{project.conf.pathN64Inst} / "bin";
  fs::path convAudio = binDir / "audioconv64";
  fs::path convWav = binDir / "tsq_conv_wav";
  fs::path convMidi = binDir / "tsq_conv_midi";
  fs::path convXM = binDir / "tsq_conv_xm";
  fs::path convSF2 = binDir / "tsq_conv_sf2";

  auto &assets = sceneCtx.project->getAssets();

  // .sf2 banks are stripped to the programs the project's MIDIs actually use,
  // so a changed MIDI also makes every bank stale
  std::string usedByMidis{};
  uint64_t newestMidiAge = 0;
  for (auto &asset : assets.getTypeEntries(Project::FileType::SEQUENCE)) {
    if(asset.conf.exclude)continue;
    if(fs::path{asset.path}.extension() == ".mid") {
      usedByMidis += " \"" + asset.path + "\"";
      newestMidiAge = std::max(newestMidiAge, Utils::FS::getFileAge(asset.path));
    }
  }

  auto procAsset = [&](const Project::AssetManagerEntry &asset)
  {
    if(asset.conf.exclude)return true;

    auto projectPath = fs::path{project.getPath()};
    auto outPath = projectPath / asset.outPath;
    fs::create_directories(outPath.parent_path());

    sceneCtx.files.push_back(Utils::FS::toUnixPath(asset.outPath));

    bool midisChanged = asset.type == Project::FileType::SOUND_FONT
      && newestMidiAge >= Utils::FS::getFileAge(outPath);
    if(!assetBuildNeeded(asset, outPath) && !midisChanged)return true;

    auto srcExt = fs::path{asset.path}.extension().string();
    std::string cmd{};

    if(asset.type == Project::FileType::AUDIO)
    {
      if(WavCompr::isStreamed(asset.conf.wavCompression.value))
      {
        // streamed .wav64 (opus/ulc), played by a StreamPlayer
        cmd = convAudio.string();
        if(asset.conf.wavForceMono.value)cmd += " --wav-mono";
        if(asset.conf.wavResampleRate.value != 0) {
          cmd += " --wav-resample " + std::to_string(asset.conf.wavResampleRate.value);
        }
        cmd += " --wav-compress " + std::to_string(asset.conf.wavCompression.value);
        cmd += " -o \"" + outPath.parent_path().string() + "\"";
        cmd += " \"" + asset.path + "\"";
      }
      else
      {
        // .tsw sample
        cmd = convWav.string();
        cmd += " \"" + asset.path + "\" \"" + outPath.string() + "\"";
        switch(asset.conf.wavCompression.value) {
          case WavCompr::VADPCM: cmd += " --vadpcm"; break;
          case WavCompr::VADPCM2: cmd += " --vadpcm2"; break;
          case WavCompr::PCM8: cmd += " --raw8"; break;
          default: cmd += " --raw"; break;
        }
        if(asset.conf.wavForceMono.value)cmd += " --mono";
        if(asset.conf.wavLoop.value)cmd += " --loop";
        if(asset.conf.wavResampleRate.value != 0) {
          Utils::Logger::log("Sample-rate setting is ignored for .tsw samples: " + asset.path, Utils::Logger::LEVEL_WARN);
        }
      }
    }
    else if(asset.type == Project::FileType::SEQUENCE)
    {
      if(srcExt == ".mid") {
        cmd = convMidi.string();
        cmd += " \"" + asset.path + "\" \"" + outPath.string() + "\"";
      } else {
        // XM emits <stem>.tsq + a companion <stem>.tsf + samples
        cmd = convXM.string();
        cmd += " \"" + asset.path + "\" \"" + outStem(outPath) + "\"";
      }
    }
    else if(asset.type == Project::FileType::SOUND_FONT)
    {
      cmd = convSF2.string();
      cmd += " \"" + asset.path + "\" \"" + outStem(outPath) + "\"";
      if(!usedByMidis.empty())cmd += " --used-by" + usedByMidis;
      if(asset.conf.wavResampleRate.value != 0) {
        cmd += " --max-rate " + std::to_string(asset.conf.wavResampleRate.value);
      }
    }
    else {
      return true;
    }

    return sceneCtx.toolchain.runCmdSyncLogged(cmd);
  };

  for (auto &asset : assets.getTypeEntries(Project::FileType::AUDIO)) {
    if(!procAsset(asset))return false;
  }
  for (auto &asset : assets.getTypeEntries(Project::FileType::SOUND_FONT)) {
    if(!procAsset(asset))return false;
  }
  for (auto &asset : assets.getTypeEntries(Project::FileType::SEQUENCE)) {
    if(!procAsset(asset))return false;
  }
  return true;
}
