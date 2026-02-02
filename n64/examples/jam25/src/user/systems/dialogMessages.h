/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <lib/types.h>

#define MSG(ID, NEXT_ID, IMAGE_ID, TEXT) \
  if (messageId == ID##_hash) { \
    return { TEXT, NEXT_ID, IMAGE_ID, 0 }; \
  }

#define MSG_MODAL(ID, IMAGE_ID, TEXT) \
  if (messageId == ID##_hash) { \
    return { TEXT, 0, IMAGE_ID, 1 }; \
  }

namespace P64::User::Dialog
{
  struct Message
  {
    const char* const text;
    uint32_t nextMessageId;
    uint8_t imageId;
    uint8_t isModal;
  };

  #define CTRL_SLOW "\x01"

  constexpr uint8_t IMG_HEAD = 0;
  constexpr uint8_t NON_MODAL = 0;
  constexpr uint8_t MODAL = 1;

  constexpr Message getMessage(uint32_t messageId)
  {

// ======== Menu/Intro Sequence ========//

    MSG("IntroStart", "IntroStart_1"_hash, IMG_HEAD,
      "Hey you!\n"
      "Now wait a moment\x01...\x01"
    );
    MSG("IntroStart_1", "IntroStart_2"_hash, IMG_HEAD,
      "You must be the\x01 worker\x01 i acquired.\n"
      "Just lazily floating around, huh?"
    );
    MSG("IntroStart_2", 0, IMG_HEAD,
      "My Kingdom in shambles,\n"
      "and you do nothing."
    );

    MSG_MODAL("IntroQ", IMG_HEAD,
      "\x01So...\x01 let's get going if you don't mind?\n"
      "Ok|No!"
    );
      MSG("IntroQ_Yes", 0, IMG_HEAD,
        "Splendid!\n"
        "Follow me then."
      );

      MSG_MODAL("IntroQ_No", IMG_HEAD,
        "Hmm, you think you have a choice?\n"
        "Yeah!|No..."
      );
        MSG("IntroQ_No_Yes", 0, IMG_HEAD,
          "Well, think again!\n"
          "So let me ask you once more."
        );
        MSG("IntroQ_No_No", 0, IMG_HEAD,
          "At last a sensible answer!\n"
          "So let me ask you again."
        );

// ======== Game Intro Map ========//
    MSG("MapTut_Start", "MapTut_Start_1"_hash, IMG_HEAD,
      "And here we are.\n"
      "Look how empty everything is..."
    );
    MSG("MapTut_Start_1", "MapTut_Start_2"_hash, IMG_HEAD,
      "See those black orbs over there?\n"
      "One day they just appeared..."
    );
    MSG("MapTut_Start_2", "MapTut_Start_3"_hash, IMG_HEAD,
      "Swallowing everything in their path.\n"
      "All they left where those fragments."
    );
    MSG("MapTut_Start_3", "MapTut_Start_4"_hash, IMG_HEAD,
      "If you collect enough of them,\n"
      "maybe we can fix this mess."
    );
    MSG("MapTut_Start_4", 0, IMG_HEAD,
      "But enough talk.\n"
      "Let's get going!"
    );

  // ======== General ========//
    MSG("Goal", 0, IMG_HEAD,
      "Not bad i must say...\n"
      "But we still have work to do."
    );

    return {};
  }
}

#undef MESSAGE_ENTRY