/**
* @copyright 2023 - Max Bebök
* @license MIT
*/
#pragma once

#include <cstdio>
#include <unordered_map>
#include <bit>
#include <stdexcept>
#include <cstdint>
#include <filesystem>

#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

namespace fs = std::filesystem;

namespace Utils
{
  enum DataType
  {
    u8, s8, u16, s16, u32, s32,
    f32, string,
    ASSET_SPRITE,
    OBJECT_REF
  };

  class BinaryFile
  {
    private:
      std::unordered_map<std::string, uint32_t> patchMap{};
      std::vector<uint32_t> posStack{};
      std::vector<uint8_t> data{};
      uint32_t dataPos{};
      uint32_t dataSize{};

      void writeRaw(const uint8_t* ptr, size_t size) {
        if(dataPos+size > data.size()) {
          data.resize(data.size() + size);
        }
        for(size_t i=0; i<size; ++i) {
          data[dataPos++] = ptr[i];
        }
        dataSize = std::max(dataSize, dataPos);
      }

    public:

      void skip(uint32_t bytes) {
        for(uint32_t i=0; i<bytes; ++i) {
          write<uint8_t>(0);
        }
      }

      template<typename T>
      void write(T value) {
        if constexpr (std::is_same_v<T, float>) {
          uint32_t val = std::byteswap(std::bit_cast<uint32_t>(value));
          writeRaw(reinterpret_cast<uint8_t*>(&val), sizeof(T));
        } else {
          auto val = std::byteswap(value);
          writeRaw(reinterpret_cast<uint8_t*>(&val), sizeof(T));
        }
      }

      void write(const std::string &str) {
        writeChars(str.c_str(), str.size());
      }

      void write(const glm::vec3 &v) {
        write(v.x);
        write(v.y);
        write(v.z);
      }

      void write(const glm::vec4 &v) {
        write(v.x);
        write(v.y);
        write(v.z);
        write(v.w);
      }

      void writeRGBA(const glm::vec4 &color) {
        write<uint8_t>(color.r * 255);
        write<uint8_t>(color.g * 255);
        write<uint8_t>(color.b * 255);
        write<uint8_t>(color.a * 255);
      }

      void writeRGB(const glm::vec4 &color) {
        write<uint8_t>(color.r * 255);
        write<uint8_t>(color.g * 255);
        write<uint8_t>(color.b * 255);
      }

      void writeChars(const char* str, size_t len) {
        for(size_t i=0; i<len; ++i)write<uint8_t>(str[i]);
      }

      template<typename T>
      void writeArray(const T* arr, size_t count) {
        for(size_t i=0; i<count; ++i) {
          write(arr[i]);
        }
      }

      void writeAs(const std::string &str, DataType type) {
        switch(type) {
          case f32: write<float>(std::stof(str)); break;
          case u32: write<uint32_t>(std::stoul(str)); break;
          case s32: write<int32_t>(std::stol(str)); break;
          case u16: write<uint16_t>(std::stoul(str)); break;
          case s16: write<int16_t>(std::stol(str)); break;
          case u8: write<uint8_t>(std::stoul(str)); break;
          case s8: write<int8_t>(std::stol(str)); break;
          case OBJECT_REF: write<uint32_t>(std::stoul(str)); break;
          case string:
            for(char c : str)write<uint8_t>(c);
            write<uint8_t>(0);
            break;
          default:
            throw std::runtime_error("unsupported data type");
        }
      }

      void writeMemFile(const BinaryFile& memFile) {
        writeRaw(memFile.data.data(), memFile.dataSize);
      }

      void writeChunkPointer(char type, uint32_t offset) {
        offset = offset & 0xFF'FFFF;
        offset |= (uint32_t)type << 24;
        write(offset);
      }

      uint32_t getPos() {
        return dataPos;
      }

      void setPos(uint32_t pos) {
        dataPos = pos;
      }

      uint32_t posPush(uint32_t newPos) {
        uint32_t oldPos = getPos();
        posStack.push_back(oldPos);
        setPos(newPos);
        return oldPos;
      }

      uint32_t posPop() {
        uint32_t oldPos = getPos();
        setPos(posStack.back());
        posStack.pop_back();
        return oldPos;
      }

      void align(uint32_t alignment) {
        uint32_t pos = getPos();
        uint32_t offset = pos % alignment;
        if(offset != 0) {
          uint32_t padding = alignment - offset;
          for(uint32_t i=0; i<padding; ++i) {
            write<uint8_t>(0);
          }
        }
      }

      uint32_t getSize() const {
        return dataSize;
      }

      void writeToFile(const fs::path &filename) {
        FILE* file = fopen(filename.string().c_str(), "wb");
        fwrite(data.data(), 1, dataSize, file);
        fflush(file);
        fclose(file);
      }

      std::vector<uint8_t> &getData() {
        data.resize(dataSize);
        return data;
      }
  };
}
