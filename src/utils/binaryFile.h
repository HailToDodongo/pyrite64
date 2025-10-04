/**
* @copyright 2023 - Max Bebök
* @license MIT
*/
#pragma once

#include <cstdio>
#include <unordered_map>
#include <bit>

#include "color.h"

namespace Utils
{
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

      void writeRGBA(const Color &color) {
        write<uint8_t>(color.r * 255);
        write<uint8_t>(color.g * 255);
        write<uint8_t>(color.b * 255);
        write<uint8_t>(color.a * 255);
      }

      void writeRGB(const Color &color) {
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

      uint32_t posPush() {
        uint32_t oldPos = getPos();
        posStack.push_back(oldPos);
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

      void writeToFile(const std::string &filename) {
        FILE* file = fopen(filename.c_str(), "wb");
        fwrite(data.data(), 1, dataSize, file);
        fclose(file);
      }
  };
}
