/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <libdragon.h>
#include <utility>
#include <vector>
#include <functional>

namespace P64::Debug
{
  struct Menu;

  enum class MenuItemType : uint8_t {
    BOOL,
    U32,
    U16,
    U8,
    F32,
    ACTION,
    SUBMENU,
    RETURN,
  };

  struct MenuItem
  {
    const char *text{};
    MenuItemType type{};
    std::function<void(MenuItem&)> onChange{nullptr};
    void *value{};
    float min{-0xFFFF};
    float max{0xFFFF};
    float step{1};

    bool &getBool() const { return *static_cast<bool*>(value); }
    uint32_t &getU32() const { return *static_cast<uint32_t*>(value); }
    uint16_t &getU16() const { return *static_cast<uint16_t*>(value); }
    uint8_t &getU8() const { return *static_cast<uint8_t*>(value); }
    float &getF32() const { return *static_cast<float*>(value); }
    Menu* getMenu() const { return static_cast<Menu*>(value); }
  };

  struct Menu
  {
    typedef void (*DrawFunc)();

    std::vector<MenuItem> items{};
    DrawFunc onDraw{nullptr};
    uint32_t currIndex{0};
    Menu *activSubMenu{nullptr};

    void update();
    void draw();

    Menu& add(const char* name, bool &value) {
      items.push_back({
        .text = name,
        .type = MenuItemType::BOOL,
        .value = &value
      });
      return *this;
    }
    Menu& add(const char* name, uint32_t &value, uint32_t min = 0, uint32_t max = 0xFFFF, uint32_t step = 1) {
      items.push_back({
        .text = name,
        .type = MenuItemType::U32,
        .value = &value,
        .min = (float)min,
        .max = (float)max,
        .step = (float)step
      });
      return *this;
    }
    Menu& add(const char* name, uint16_t &value, uint16_t min = 0, uint16_t max = 0xFFFF, uint16_t step = 1) {
      items.push_back({
        .text = name,
        .type = MenuItemType::U16,
        .value = &value,
        .min = (float)min,
        .max = (float)max,
        .step = (float)step
      });
      return *this;
    }
    Menu& add(const char* name, uint8_t &value, uint8_t min = 0, uint8_t max = 0xFF, uint8_t step = 1) {
      items.push_back({
        .text = name,
        .type = MenuItemType::U8,
        .value = &value,
        .min = (float)min,
        .max = (float)max,
        .step = (float)step
      });
      return *this;
    }
    Menu& add(const char* name, float &value, float min = -0xFFFF, float max = 0xFFFF, float step = 1) {
      items.push_back({
        .text = name,
        .type = MenuItemType::F32,
        .value = &value,
        .min = min,
        .max = max,
        .step = step
      });
      return *this;
    }

    Menu& add(const char* name, std::function<void(MenuItem&)> action) {
      items.push_back({name, MenuItemType::ACTION, std::move(action)});
      return *this;
    }

    Menu& add(const char* name, Menu& subMenu);
  };
}