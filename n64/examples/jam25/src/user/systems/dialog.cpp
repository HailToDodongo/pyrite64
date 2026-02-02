/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "dialog.h"
#include <assets/assetManager.h>
#include <vi/swapChain.h>

#include "../../p64/assetTable.h"
#include "../globals.h"

namespace
{
  constexpr float posCenterY = 320 / 2;
  constexpr rdpq_textparms_t TEXT_CENTER{
    .width = 320,
    .align = ALIGN_LEFT,
    .disable_aa_fix = true
  };

  constinit sprite_t *texBox{};
  constinit sprite_t *head{};

  constexpr float bxHeight = 42.0f;
  constinit float posYOrg = 240 - bxHeight - 30;
  constinit float bxWidth = 280.0f;
  constinit float posX = 0;

  constinit float timer = 0;
  constinit float blinkTimer = 0;
  constinit bool slowMode = false;

  struct DialogMsg
  {
    std::string text{};
    std::string optionYes{};
    std::string optionNo{};

    char replacedChar{};
    uint32_t currPos{};
    uint32_t nextMessageId{0};
    bool isModal{false};

    [[nodiscard]] bool isReady() const {
      return currPos >= text.length();
    }

    void nextChar() {
      if(currPos >= text.length())return;

      if(replacedChar == CTRL_SLOW[0]) {
        slowMode = !slowMode;
      }

      text[currPos] = replacedChar;
      ++currPos;

      replacedChar = text[currPos];
      text[currPos] = '\0';
    }
  };

  float messageFadeTimer = 0;
  float messageFadeInTimer = 0;
  bool showingMessage = false;
  volatile bool processingMessage = false;
  constinit DialogMsg currMsg{};
  constinit uint8_t selOption{};

  constinit volatile int result{0};
}

namespace P64::User::Dialog
{
  constinit rspq_block_t *dplBox{};

  void init()
  {
    texBox = (sprite_t*)AssetManager::getByIndex("ui/textbox.i8.sprite"_asset);
    head = (sprite_t*)AssetManager::getByIndex("ui/head00.rgba16.sprite"_asset);

    posX = posCenterY - bxWidth / 2;

    rspq_block_begin();
      rdpq_set_mode_standard();
      rdpq_mode_begin();
        rdpq_mode_alphacompare(1);
        rdpq_mode_combiner(RDPQ_COMBINER1((0,0,0,PRIM), (PRIM,0,TEX0,0)));
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
      rdpq_mode_end();

      rdpq_set_prim_color((color_t){11, 11, 11, 0xCC});
      rdpq_sprite_upload(TILE0, texBox, nullptr);
    dplBox = rspq_block_end();

    //showMessage(1);
  }

  void destroy()
  {
    rspq_block_free(dplBox);
    dplBox = nullptr;
    texBox = nullptr;
  }

  constinit joypad_8way_t lastDir{JOYPAD_8WAY_NONE};

  bool update()
  {
    auto pressed = joypad_get_buttons_pressed(JOYPAD_PORT_1);

    timer += VI::SwapChain::getDeltaTime();
    float charDelay = slowMode ? 0.12f : 0.02f;
    if(timer > charDelay)
    {
      timer = 0;
      currMsg.nextChar();
    }

    // modal selection
    if(currMsg.isModal && currMsg.isReady())
    {
      auto dir = joypad_get_direction(JOYPAD_PORT_1, JOYPAD_2D_ANY);
      if(lastDir != dir)
      {
        lastDir = dir;
        if(dir == JOYPAD_8WAY_LEFT || dir == JOYPAD_8WAY_RIGHT) {
          selOption = (selOption + 1) % 2;
        }
      }
    }

    if(pressed.a)
    {
      if(currMsg.isReady())
      {
        if(currMsg.nextMessageId == 0)
        {
          //showingMessage = false;
          if (processingMessage) {
            messageFadeTimer = 1.0f;
            result = selOption;
          }

          processingMessage = false;
        } else {
          showMessage(currMsg.nextMessageId);
        }
      } else {
        while(!currMsg.isReady())currMsg.nextChar();
      }
    }

    if(messageFadeTimer != 0.0f)
    {
      messageFadeTimer -= VI::SwapChain::getDeltaTime() * 4.0f;
      if(messageFadeTimer <= 0.0f)
      {
        messageFadeTimer = 0.0f;
        showingMessage = false;
      }
    }

    return showingMessage;
  }

  void drawBoxBg(float posX, float posY, float width, float height)
  {
    // texture is only the corner, draw 4 times for each corner and extend the clamped texture
    rdpq_texture_rectangle(TILE0, posX,         posY,          posX + width/2,    posY + height/2,     0, 0);
    rdpq_texture_rectangle(TILE0, posX,         posY + height, posX + width/2,    posY + height/2 - 1, 0, 0);
    rdpq_texture_rectangle(TILE0, posX + width, posY,          posX + width/2 -1, posY + height/2,     0, 0);
    rdpq_texture_rectangle(TILE0, posX + width, posY + height, posX + width/2 -1, posY + height/2 - 1, 0, 0);
  }

  void draw()
  {
    if(!showingMessage)return;
    drawBoxBgSart();

    float posY = posYOrg;
    if (messageFadeTimer != 0.0f) {
      posY += (1.0f - messageFadeTimer) * 66.0f;
    }

    drawBoxBg(posX, posY, bxWidth, bxHeight);

    rdpq_mode_begin();
      rdpq_mode_alphacompare(128);
      rdpq_mode_combiner(RDPQ_COMBINER_TEX);
      rdpq_mode_blender(0);
    rdpq_mode_end();
    rdpq_set_prim_color((color_t){0x77, 0x77, 0xFF, 0xFF});

    rdpq_sprite_blit(head,
      posX + 8,
      posY + bxHeight/2 - head->height/2,
      nullptr
    );

    int textPosX = posX + 12;
    textPosX += 36; // icon

    rdpq_fontstyle_t style{.color =  {0xFF, 0xFF, 0xFF, 0xFF}};
    rdpq_font_style(const_cast<rdpq_font_t*>(rdpq_text_get_font(FONT_TEXT)), 0, &style);
    rdpq_text_printf(&TEXT_CENTER, FONT_TEXT, textPosX, posY+18, currMsg.text.data());

    if(currMsg.isModal && currMsg.isReady())
    {
      blinkTimer += VI::SwapChain::getDeltaTime();

      uint8_t alpha = (fm_sinf(blinkTimer * 7.0f) * 0.25f + 0.75f) * 255;

      // show Yes / No options
      rdpq_fontstyle_t optStyleNormal{.color =  {0x80, 0x80, 0x80, 0xFF}};
      rdpq_fontstyle_t optStyleSelected{.color =  {0xCC, 0xCC, 0xFF, alpha}};
      rdpq_font_style(const_cast<rdpq_font_t*>(rdpq_text_get_font(FONT_TEXT)), 0, selOption == 0 ? &optStyleSelected : &optStyleNormal);
      rdpq_text_printf(&TEXT_CENTER, FONT_TEXT, textPosX + 50, posY + bxHeight - 8, currMsg.optionYes.c_str());
      rdpq_font_style(const_cast<rdpq_font_t*>(rdpq_text_get_font(FONT_TEXT)), 0, selOption == 1 ? &optStyleSelected : &optStyleNormal);
      rdpq_text_printf(&TEXT_CENTER, FONT_TEXT, textPosX + 90, posY + bxHeight - 8, currMsg.optionNo.c_str());
    } else {
      blinkTimer = 0;
    }
  }

  int showMessage(uint32_t messageId)
  {
    result = 0;
    auto msg = getMessage(messageId);
    currMsg.text = msg.text;
    currMsg.currPos = 0;

    currMsg.replacedChar = currMsg.text[0];
    currMsg.text[0] = '\0';
    currMsg.nextMessageId = msg.nextMessageId;
    currMsg.isModal = msg.isModal != 0;

    if(currMsg.isModal) {
      auto idxSecondLine = currMsg.text.find('\n');
      auto sepPos = currMsg.text.find('|', idxSecondLine);

      if(sepPos != std::string::npos) {
        currMsg.optionYes = currMsg.text.substr(sepPos + 1);
        currMsg.optionNo = currMsg.text.substr(idxSecondLine + 1, sepPos - idxSecondLine -1);
        currMsg.text.erase(sepPos, currMsg.optionNo.length() +1);
      }

      // limit actual text to remove second line
      currMsg.text.erase(idxSecondLine);
    }

    selOption = 0;
    showingMessage = true;
    processingMessage = true;
    messageFadeTimer = 0;

    if(coro_get_current() != nullptr)
    {
      while(processingMessage) {
        coro_yield();
      }
      return result;
    }

    return 0;
  }
}
