/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "../../include/debug/menu.h"
#include "debug/debugDraw.h"

namespace
{
  int holdFrames = 0;
  enum class HoldDir { NONE, LEFT, RIGHT };
  HoldDir holdDir = HoldDir::NONE;
}

void P64::Debug::Menu::update()
{
  if(activSubMenu) {
    return activSubMenu->update();
  }

  auto btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);
  auto held = joypad_get_buttons_held(JOYPAD_PORT_1);

  if(btn.d_up && !held.l)
  {
    if(currIndex == 0)currIndex = items.size() - 1;
    else --currIndex;
  }
  if(btn.d_down)++currIndex;
  if(currIndex > items.size() - 1)currIndex = 0;

  // --- Hold logic for left/right ---
  bool left = held.d_left;
  bool right = held.d_right;
  static bool prevLeft = false;
  static bool prevRight = false;

  // Detect direction change or release
  if((!left && !right) || (left && !prevLeft && right && !prevRight) || (left && !right && holdDir == HoldDir::RIGHT) || (right && !left && holdDir == HoldDir::LEFT)) {
    holdFrames = 0;
    holdDir = HoldDir::NONE;
  }

  // Set hold direction
  if(left && !right) holdDir = HoldDir::LEFT;
  else if(right && !left) holdDir = HoldDir::RIGHT;

  bool doChange = false;
  if(left || right) {
    if(holdFrames == 0) {
      doChange = true; // Initial press
    } else {
      // After threshold, accelerate
      constexpr int threshold = 20;
      constexpr int fastStep = 2;
      if(holdFrames > threshold && ((holdFrames - threshold) % fastStep == 0)) {
        doChange = true;
      }
    }
    holdFrames++;
  }

  if(doChange && (left || right)) {
    auto &item = items[currIndex];
    switch(item.type) {
      case MenuItemType::BOOL:
        item.getBool() = !item.getBool();
        break;
      case MenuItemType::U32:
        if(left)item.getU32() -= (uint32_t)item.step;
        if(right)item.getU32() += (uint32_t)item.step;
        if(item.getU32() < (uint32_t)item.min)item.getU32() = (uint32_t)item.min;
        if(item.getU32() > (uint32_t)item.max)item.getU32() = (uint32_t)item.max;
        break;
      case MenuItemType::U16:
        if(left)item.getU16() -= (uint16_t)item.step;
        if(right)item.getU16() += (uint16_t)item.step;
        if(item.getU16() < (uint16_t)item.min)item.getU16() = (uint16_t)item.min;
        if(item.getU16() > (uint16_t)item.max)item.getU16() = (uint16_t)item.max;
        break;
      case MenuItemType::U8:
        if(left)item.getU8() -= (uint8_t)item.step;
        if(right)item.getU8() += (uint8_t)item.step;
        if(item.getU8() < (uint8_t)item.min)item.getU8() = (uint8_t)item.min;
        if(item.getU8() > (uint8_t)item.max)item.getU8() = (uint8_t)item.max;
        break;
      case MenuItemType::F32:
        if(left)item.getF32() -= item.step;
        if(right)item.getF32() += item.step;
        if(item.getF32() < item.min)item.getF32() = item.min;
        if(item.getF32() > item.max)item.getF32() = item.max;
        break;
      default: break;
    }
    if(item.onChange)item.onChange(item);
  }
  prevLeft = left;
  prevRight = right;
}

void P64::Debug::Menu::draw()
{
  if(activSubMenu) {
    return activSubMenu->draw();
  }

  uint16_t posX = 24;
  uint16_t posY = 30;

  // Menu
  for(auto &item : items)
  {
    bool isSel = currIndex == (uint32_t)(&item - &items[0]);
    if(isSel) {
      Debug::setBgColor({0, 0, 0x55, 0xFF});
      Debug::print(posX, posY, DEBUG_CHAR_ARROW);
      Debug::setBgColor();
    }

    int px = posX + 10;

    switch(item.type) {
      case MenuItemType::U32:
      case MenuItemType::U16:
      case MenuItemType::U8:
      case MenuItemType::F32:
        px = Debug::print(px, posY, DEBUG_CHAR_VALUE " ");
        px = Debug::print(px, posY, item.text);

        Debug::setColor(
          item.step == 0.0f
            ? color_t{0xBB, 0xBB, 0xBB, 0xFF}
            : color_t{0xBB, 0xBB, 0xFF, 0xFF}
        );

        if(item.type == MenuItemType::U32)Debug::printf(px, posY, ": %lu", item.getU32());
        if(item.type == MenuItemType::U16)Debug::printf(px, posY, ": %u", item.getU16());
        if(item.type == MenuItemType::U8)Debug::printf(px, posY, ": %u", item.getU8());
        if(item.type == MenuItemType::F32)Debug::printf(px, posY, ": %.4f", (double)item.getF32());

        Debug::setColor();
        break;
      case MenuItemType::BOOL:
      {
        if(item.getBool())Debug::setColor({0x22, 0xAA, 0x22, 0xFF});
        px = Debug::print(px, posY, item.getBool() ? (DEBUG_CHAR_CHECK_ON " ") : (DEBUG_CHAR_CHECK_0FF " "));
        if(item.getBool())Debug::setColor();
        Debug::print(px, posY, item.text);
      }
      break;
      case MenuItemType::ACTION:
        px = Debug::print(px, posY, DEBUG_CHAR_FUNC " ");
        Debug::print(px, posY, item.text);
        break;
      case MenuItemType::SUBMENU:
        px = Debug::print(px, posY, DEBUG_CHAR_DIR " ");
        Debug::print(px, posY, item.text);
        break;
      case MenuItemType::RETURN:
        Debug::setColor({0xBB, 0xBB, 0xBB, 0xFF});
        Debug::print(px, posY, DEBUG_CHAR_RETURN " Back");
        Debug::setColor();
        break;
    }
    posY += 9;
  }

  if(onDraw)onDraw();
}

P64::Debug::Menu & P64::Debug::Menu::add(const char* name, Menu &subMenu)
{
  auto menuPtr = &subMenu;
  items.push_back({name, MenuItemType::SUBMENU, [this, menuPtr](auto &item) {
    activSubMenu = menuPtr;
    //activSubMenu->currIndex = 1; // skip return item
  }});
  subMenu.items.insert(subMenu.items.begin(), {"", MenuItemType::RETURN, [this](auto &item) {
    activSubMenu = nullptr;
  }});
  return *this;
}
