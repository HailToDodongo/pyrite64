/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <array>
#include <vector>

// Option tables for the N64 ROM header / EverDrive config, shared by the
// project-settings UI (combo boxes) and the build step (Makefile flags).
// `label` is what the editor shows, `token` is the value passed to the libdragon
// Makefile vars (n64tool / ed64romconfig). An empty token means "emit no flag".
namespace Project::RomMeta
{
  struct Option { const char* label; const char* token; };

  inline constexpr std::array<Option, 6> CATEGORY = {{
    {"(default)", ""}, {"N - Game Pak", "N"}, {"D - 64DD Disk", "D"},
    {"C - Cart + Disk", "C"}, {"E - Expansion", "E"}, {"Z - Aleck64", "Z"},
  }};

  inline constexpr std::array<Option, 20> REGION = {{
    {"(default)", ""}, 
    {"H - Netherlands", "H"},
    {"S - Spain", "S"},
    {"B - Brazil", "B"},
    {"I - Italy", "I"},
    {"U - Australia", "U"},
    {"C - China", "C"},
    {"J - Japan", "J"},
    {"W - Scandinavia", "W"},
    {"D - Germany", "D"},
    {"K - Korea", "K"},
    {"X - Europe", "X"},
    {"E - North America", "E"},
    {"L - Gateway 64 (PAL)", "L"},
    {"Y - Europe", "Y"},
    {"F - France", "F"},
    {"N - Canada", "N"},
    {"Z - Europe", "Z"},
    {"G - Gateway 64 (NTSC)", "G"},
    {"P - Europe", "P"},
  }};

  inline constexpr std::array<Option, 7> SAVETYPE = {{
    {"None", "none"}, {"EEPROM 4k", "eeprom4k"}, {"EEPROM 16k", "eeprom16k"},
    {"SRAM 256k", "sram256k"}, {"SRAM 768k", "sram768k"}, {"SRAM 1m", "sram1m"},
    {"FlashRAM", "flashram"},
  }};

  inline constexpr std::array<Option, 10> CONTROLLER = {{
    {"N64", "n64"}, {"N64 + Rumble Pak", "n64,pak=rumble"},
    {"N64 + Controller Pak", "n64,pak=controller"}, {"N64 + Transfer Pak", "n64,pak=transfer"},
    {"None", "none"}, {"Mouse", "mouse"}, {"VRU", "vru"}, {"GameCube", "gamecube"},
    {"Randnet Keyboard", "randnetkeyboard"}, {"GameCube Keyboard", "gamecubekeyboard"},
  }};

  // Returns the token for a given option index, or "" if out of range.
  template<size_t N>
  inline const char* tokenOf(const std::array<Option, N> &opts, int idx) {
    return (idx >= 0 && idx < (int)N) ? opts[idx].token : "";
  }

  // Labels as a vector for ImTable::addComboBox.
  template<size_t N>
  inline std::vector<const char*> labels(const std::array<Option, N> &opts) {
    std::vector<const char*> out;
    out.reserve(N);
    for (const auto &o : opts) out.push_back(o.label);
    return out;
  }
}
