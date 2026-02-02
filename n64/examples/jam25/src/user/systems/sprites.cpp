/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "sprites.h"

#include <scene/scene.h>
#include <scene/sceneManager.h>

#include "../../p64/sceneTable.h"
#include "vi/swapChain.h"
#include "renderer/particles/ptxSprites.h"

constinit P64::PTX::Sprites* coin = nullptr;
constinit P64::PTX::Sprites* dust = nullptr;

namespace
{
  void simulateDust(P64::PTX::Sprites &sprites, float deltaTime)
  {
    sprites.simTimer += deltaTime;
    bool isStep = sprites.simTimer > 0.025f;
    if(isStep)sprites.simTimer = 0;

    auto &system = sprites.system;
    auto buff = system.getBufferS16();
    {
      for(uint32_t i=0; i<system.count; ++i) {
        auto pos = tpx_buffer_s16_get_pos(buff, i);
        int8_t *size = tpx_buffer_s16_get_size(buff, i);
        color_t *col = (color_t *)tpx_buffer_s16_get_rgba(buff, i);

        if(isStep)pos[1] += 1;
        size[0] -= 1;

        if(col->r > 1) {
          col->r -= 1;
          col->g -= 1;
          col->b -= 1;
        }

        if(size[0] <= 0) {
          system.removeParticle(i--);
        }
      }
    }
  }
}

void P64::User::Sprites::init()
{
  coin = new PTX::Sprites("rom:/ptx/coin.sprite",
    PTX::Sprites::Conf{
      .maxSize = 256,
      .isRotating = false,
      .noRng = false,
    });
  coin->setColor({0xFF, 0xEF, 0x76, 0xFF});

  coinPart = new PTX::Sprites("rom:/ptx/coinPart.sprite",
    PTX::Sprites::Conf{
      .maxSize = 256,
      .isRotating = true,
      .noRng = false,
    });
  coinPart->setColor({0xFF, 0xEF, 0x76, 0xFF});

  dust = new PTX::Sprites("rom:/ptx/swirl.sprite",
    PTX::Sprites::Conf{
      .maxSize = 256,
      .isRotating = true,
      .noRng = false,
    });
  dust->setColor({0xCC, 0xCC, 0xCC, 0xFF});
}

void P64::User::Sprites::destroy()
{
  delete coin; coin = nullptr;
  delete coinPart; coinPart = nullptr;
  delete dust; dust = nullptr;
}

void P64::User::Sprites::reset()
{
  coin->clear();
  coinPart->clear();
  //dust->clear();
}

void P64::User::Sprites::draw()
{
  float deltaTime = VI::SwapChain::getDeltaTime();

  // @TODO: fix this bs in t3d:
  float far = P64::SceneManager::getCurrent().getActiveCamera().far;
  far = 1.0f / (far / 500.0f);
  far *= 64;
  tpx_state_set_base_size((uint16_t)far);
  coin->draw(deltaTime);

  coinPart->draw(deltaTime);

  simulateDust(*dust, deltaTime);
  dust->draw(deltaTime);
}
