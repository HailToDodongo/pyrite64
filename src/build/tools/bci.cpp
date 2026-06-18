/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "bci.h"
#include <filesystem>
#include <vector>
#include <array>
#include <cmath>
#include <limits>
#include <iostream>
#include "lodepng.h"

using namespace std;

namespace
{
  struct Color {
    int r, g, b;
    Color operator+(const Color& other) const {
      return {r + other.r, g + other.g, b + other.b};
    }
    Color operator/(int val) const {
      return {r / val, g / val, b / val};
    }
    double distance(const Color& other) const {
      return sqrt(pow(r - other.r, 2) + pow(g - other.g, 2) + pow(b - other.b, 2));
    }
    bool operator==(const Color&other) const {
      return r == other.r && g == other.g && b == other.b;
    }
    uint16_t toRGBA555() const {
      return ((r << 8) & 0b11111'00000'00000'0)
       |     ((g << 3) & 0b00000'11111'00000'0)
       |     ((b >> 2) & 0b00000'00000'11111'0);
    }
  };

  using Block = array<Color, 16>;  // 4x4 block of pixels
  using Palette = array<Color, 4>; // 4-color palette
  using Indices = array<int, 16>;  // Indices for each pixel in the block

  // Initialize K-means with random colors from the block
  void initialize_palette(const Block& block, Palette& palette) {
    for (int i = 0; i < 4; ++i) {
      palette[i] = block[rand() % 16];
    }
  }

  // Assign each pixel to the nearest palette color
  Indices assign_clusters(const Block& block, const Palette& palette) {
    Indices assignments;
    for (int i = 0; i < 16; ++i) {
      double min_dist = numeric_limits<double>::max();
      int best_cluster = 0;
      for (int j = 0; j < 4; ++j) {
        double dist = block[i].distance(palette[j]);
        if (dist < min_dist) {
          min_dist = dist;
          best_cluster = j;
        }
      }
      assignments[i] = best_cluster;
    }
    return assignments;
  }

  // Update palette colors based on assignments
  void update_palette(const Block& block, Palette& palette, const Indices& assignments) {
    array<Color, 4> new_colors = {};
    array<int, 4> counts = {};

    for (int i = 0; i < 16; ++i) {
      int cluster = assignments[i];
      new_colors[cluster] = new_colors[cluster] + block[i];
      counts[cluster]++;
    }

    for (int i = 0; i < 4; ++i) {
      if (counts[i] > 0) {
        palette[i] = new_colors[i] / counts[i];
      }
    }
  }

  // K-means clustering to generate a 4-color palette and indices
  pair<Palette, Indices> kmeansPalette(const Block& block, int max_iters = 100) {
    Palette palette;
    initialize_palette(block, palette);
    Indices assignments;

    for (int iter = 0; iter < max_iters; ++iter) {
      assignments = assign_clusters(block, palette);
      Palette new_palette = palette;
      update_palette(block, new_palette, assignments);

      if (palette == new_palette) break; // Converged
      palette = new_palette;
    }

    return {palette, assignments};
  }
}

bool Build::BCI::convertPNG(const std::string &pathInPNG, const std::string &pathOutBCI)
{
  vector<unsigned char> image;
  unsigned width, height;

  auto *pFile = fopen(pathOutBCI.c_str(), "wb");

  auto writeU16 = [pFile](uint16_t val) {
    uint8_t high = (val >> 8) & 0xFF;
    uint8_t low = val & 0xFF;
    fwrite(&high, 1, 1, pFile);
    fwrite(&low, 1, 1, pFile);
  };
  [[maybe_unused]] auto writeU8 = [pFile](uint8_t val) {
    fwrite(&val, 1, 1, pFile);
  };
  [[maybe_unused]] auto writeU32 = [pFile](uint32_t val) {
    uint8_t b0 = (val >> 24) & 0xFF;
    uint8_t b1 = (val >> 16) & 0xFF;
    uint8_t b2 = (val >> 8) & 0xFF;
    uint8_t b3 = val & 0xFF;
    fwrite(&b0, 1, 1, pFile);
    fwrite(&b1, 1, 1, pFile);
    fwrite(&b2, 1, 1, pFile);
    fwrite(&b3, 1, 1, pFile);
  };
  auto writeU64 = [pFile](uint64_t val) {
    uint8_t b0 = (val >> 56) & 0xFF;
    uint8_t b1 = (val >> 48) & 0xFF;
    uint8_t b2 = (val >> 40) & 0xFF;
    uint8_t b3 = (val >> 32) & 0xFF;
    uint8_t b4 = (val >> 24) & 0xFF;
    uint8_t b5 = (val >> 16) & 0xFF;
    uint8_t b6 = (val >> 8) & 0xFF;
    uint8_t b7 = val & 0xFF;
    fwrite(&b0, 1, 1, pFile);
    fwrite(&b1, 1, 1, pFile);
    fwrite(&b2, 1, 1, pFile);
    fwrite(&b3, 1, 1, pFile);
    fwrite(&b4, 1, 1, pFile);
    fwrite(&b5, 1, 1, pFile);
    fwrite(&b6, 1, 1, pFile);
    fwrite(&b7, 1, 1, pFile);
  };

  unsigned error = lodepng::decode(image, width, height, pathInPNG);
  if (error) {
    cerr << "PNG loading error: " << lodepng_error_text(error) << endl;
    return false;
  }

  for (unsigned y = 0; y < height; y += 4) {
    for (unsigned x = 0; x < width; x += 4) {
      Block block;
      for (int i = 0; i < 16; ++i) {
        unsigned px = x + (i % 4);
        unsigned py = y + (i / 4);
        if (px >= width || py >= height) continue;
        unsigned index = 4 * (py * width + px);
        block[i] = {image[index], image[index + 1], image[index + 2]};
      }

      auto [palette, indices] = kmeansPalette(block);

      // Note: the first index must be 0b00 or 0b01 due to runtime opt.
      // If that is not the case, swap the colors and indices
      if(indices[15] != 0) {
        auto replA = indices[15];

        auto tmp = palette[replA];
        palette[replA] = palette[0];
        palette[0] = tmp;

        for(uint32_t i=0; i<16; ++i) {
          if(indices[i] == replA)indices[i] = 0;
          else if(indices[i] == 0)indices[i] = replA;
        }
      }

      writeU16(palette[0].toRGBA555());
      writeU16(palette[1].toRGBA555());
      writeU16(palette[2].toRGBA555());
      writeU16(palette[3].toRGBA555());

      uint64_t packedIndex = 0;
      for(int i=0; i<16; ++i) {
        packedIndex <<= 2;
        packedIndex |= indices[15-i];
      }
      writeU64(packedIndex << 33);
    }
  }

  fclose(pFile);
  return true;
}
