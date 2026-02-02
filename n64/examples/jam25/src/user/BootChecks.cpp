#include "globals.h"
#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "systems/screenFade.h"
#include "../p64/sceneTable.h"

extern "C" {
  extern void* __mi_memset32(void *ptr, uint32_t value, size_t len);
}

namespace
{
  bool miTest()
  {
    uint32_t test[4]{};
    __mi_memset32(&test, 0x12345678, sizeof(test));
    if(test[0] != 0x12345678 || test[1] != 0x12345678 || test[2] != 0x12345678 || test[3] != 0x12345678) {
      return false;
    }
    return true;
  }
}

namespace P64::Script::C896248FAFED4CBF
{
  P64_DATA(
    [[P64::Name("Expansion Pack")]]
    AssetRef<sprite_t> texExpPack;

    bool failedExpPack;
    bool failedMiRepeat;
  );

  void initDelete(Object& obj, Data *data, bool isDelete)
  {
    if(isDelete) {
      // do cleanup
      return;
    }

    data->failedExpPack = !is_memory_expanded();
    if(data->failedExpPack)return;

    data->failedMiRepeat = !miTest() && !sys_bbplayer();
    if(data->failedMiRepeat)return;

    SceneManager::load("Boot-Logos"_scene);
    obj.remove();

    User::ScreenFade::setFadeState(false);
  }

  void update(Object& obj, Data *data, float deltaTime)
  {
  }

  void draw(Object& obj, Data *data, float deltaTime)
  {
    float midX = rdpq_get_attached()->width / 2;
    float posY = 54;
    rdpq_textparms_t TEXT_CENTER{
      .width = (int16_t)rdpq_get_attached()->width,
      .align = ALIGN_CENTER,
      .disable_aa_fix = true
    };

    DrawLayer::use2D();

    if(data->failedMiRepeat)
    {
      posY = 90;
      rdpq_text_printf(&TEXT_CENTER, User::FONT_TITLE, 0, posY,
        "MI-Repeat Mode not emulated!"
      );
      posY += 32;
      rdpq_text_printf(&TEXT_CENTER, User::FONT_TEXT, 0, posY,
        "Please use a more accurate emulator,\n"
        "or play on a real console.\n"
        "Known good emulators are Ares and Gopher64."
      );
    }

    if(data->failedExpPack)
    {
      auto pack = data->texExpPack.get();
      rdpq_sprite_blit(pack, midX - (pack->width/2), posY, nullptr);
      posY += data->texExpPack.get()->height + 38;

      rdpq_text_printf(&TEXT_CENTER, User::FONT_TITLE, 0, posY,
        "The Expansion Pak\n"
        "is required to play this Game"
      );
    }

    DrawLayer::useDefault();
  }
}
