#include "script/globalScript.h"
#include "script/userScript.h"
#include "scene/sceneManager.h"
#include "debug/debugDraw.h"
#include "./globals.h"

#include <vector>

constinit uint32_t P64::User::currFrame = 0;
constinit std::vector<P64::User::EvCapture> P64::User::events;

using namespace P64::User;

namespace P64::GlobalScript::C98127B5B02279B8
{
  std::vector<int8_t> testResult;
  constinit uint32_t currTest = 0;
  constinit bool testsDone = false;

  struct TestDef;
  using UpdateFunc = std::function<bool()>;
  using TestFunc = std::function<bool()>;

  struct TestDef
  {
    const char* const name{};
    uint16_t sceneId{};
    TestFunc fnUpdate{};
    TestFunc fnTest{};
  };

  void printEvent(User::EvCapture &ev) {
    debugf("frame:%ld | %s: Object %d %s (parent: %d) value: %ld\n",
      ev.frame, User::EVENT_NAMES[(uint32_t)ev.type], 
      ev.objId, ev.enabled ? "" : "<DIS>",
      ev.parentId, ev.value);
  }
  void printEvents() {
    uint32_t i=0;
    for(auto &ev : User::events) {
      debugf("Event[%02ld]: ", i); printEvent(ev);
      ++i;
    }
  }

  bool compareEvent(uint32_t idx, const User::EvCapture &expected) {
    if(idx >= User::events.size()) {
      debugf("[%ld] Expected event %ld but only got %ld events\n", idx, idx, (uint32_t)User::events.size());
      return false;
    }
    auto &actual = User::events[idx];
    if(actual.type != expected.type) {
      debugf("[%ld] Expected event type %s but got %s\n", idx, User::EVENT_NAMES[(uint32_t)expected.type], User::EVENT_NAMES[(uint32_t)actual.type]);
      return false;
    }
    if(actual.frame != expected.frame) {
      debugf("[%ld] Expected event frame %ld but got %ld\n", idx, expected.frame, actual.frame);
      return false;
    }
    if(actual.value != expected.value) {
      debugf("[%ld] Expected event value %ld but got %ld\n", idx, expected.value, actual.value);
      return false;
    }
    if(actual.objId != expected.objId) {
      debugf("[%ld] Expected event objId %d but got %d\n", idx, expected.objId, actual.objId);
      return false;
    }
    if(actual.parentId != expected.parentId) {
      debugf("[%ld] Expected event parentId %d but got %d\n", idx, expected.parentId, actual.parentId);
      return false;
    }
    if(actual.enabled != expected.enabled) {
      debugf("[%ld] Expected event enabled %d but got %d\n", idx, expected.enabled, actual.enabled);
      return false;
    }
    return true;
  }

  bool compareEvents(const std::vector<User::EvCapture> &expected) {
    if(expected.size() != User::events.size()) {
      debugf("Expected %ld events but got %ld\n", (uint32_t)expected.size(), (uint32_t)User::events.size());
      printEvents();
      return false;
    }

    for(uint32_t i=0; i<expected.size(); ++i) {
      if(!compareEvent(i, expected[i])) {
        debugf("Event comparison failed at index %ld\n", i);
        printEvents();
        return false;
      }
    }
    return true;
  }

  std::vector<User::EvCapture> DATA_TEST_NESTED{
    // Scene load
    {.type = EvType::INIT, .frame = 0, .objId = 1},
    {.type = EvType::INIT, .frame = 0, .objId = 2, .parentId = 1, .enabled = false},
    {.type = EvType::INIT, .frame = 0, .objId = 6, .parentId = 1},
    {.type = EvType::INIT, .frame = 0, .objId = 3,                .enabled = false},
    {.type = EvType::INIT, .frame = 0, .objId = 4, .parentId = 3, .enabled = false},
    {.type = EvType::INIT, .frame = 0, .objId = 5, .parentId = 4, .enabled = false},

    // first frame
    {.type = EvType::EV_READY, .frame = 1, .objId = 1},
    {.type = EvType::EV_READY, .frame = 1, .objId = 2, .parentId = 1, .enabled = false},
    {.type = EvType::EV_READY, .frame = 1, .objId = 6, .parentId = 1},
    {.type = EvType::EV_READY, .frame = 1, .objId = 3,                .enabled = false},
    {.type = EvType::EV_READY, .frame = 1, .objId = 4, .parentId = 3, .enabled = false},
    {.type = EvType::EV_READY, .frame = 1, .objId = 5, .parentId = 4, .enabled = false},

    {.type = EvType::UPDATE, .frame = 1, .objId = 1},
    {.type = EvType::UPDATE, .frame = 1, .objId = 6, .parentId = 1},

    {.type = EvType::DRAW, .frame = 1, .objId = 1},
    {.type = EvType::DRAW, .frame = 1, .objId = 6, .parentId = 1},
  };

  std::vector<TestDef> TESTS{
    {"Init Flat", 1, []{ return User::currFrame < 2; },
    []{
      return compareEvents({
        // Scene load
        {.type = EvType::INIT, .frame = 0, .objId = 1},
        {.type = EvType::INIT, .frame = 0, .objId = 2},
        {.type = EvType::INIT, .frame = 0, .objId = 3},

        // first frame
        {.type = EvType::EV_READY, .frame = 1, .objId = 1},
        {.type = EvType::EV_READY, .frame = 1, .objId = 2},
        {.type = EvType::EV_READY, .frame = 1, .objId = 3},

        {.type = EvType::UPDATE,   .frame = 1, .objId = 1},
        {.type = EvType::UPDATE,   .frame = 1, .objId = 2},
        {.type = EvType::UPDATE,   .frame = 1, .objId = 3},

        {.type = EvType::DRAW,     .frame = 1, .objId = 1},
        {.type = EvType::DRAW,     .frame = 1, .objId = 2},
        {.type = EvType::DRAW,     .frame = 1, .objId = 3},
      });
    }},
    {"Init Nested", 2, []{ return User::currFrame < 2; }, 
    []{
      return compareEvents({
        // Scene load
        {.type = EvType::INIT, .frame = 0, .objId = 1},
        {.type = EvType::INIT, .frame = 0, .objId = 2, .parentId = 1},
        {.type = EvType::INIT, .frame = 0, .objId = 3,              },
        {.type = EvType::INIT, .frame = 0, .objId = 4, .parentId = 3},
        {.type = EvType::INIT, .frame = 0, .objId = 5, .parentId = 4},

        // first frame
        {.type = EvType::EV_READY, .frame = 1, .objId = 1},
        {.type = EvType::EV_READY, .frame = 1, .objId = 2, .parentId = 1},
        {.type = EvType::EV_READY, .frame = 1, .objId = 3},
        {.type = EvType::EV_READY, .frame = 1, .objId = 4, .parentId = 3},
        {.type = EvType::EV_READY, .frame = 1, .objId = 5, .parentId = 4},

        {.type = EvType::UPDATE, .frame = 1, .objId = 1},
        {.type = EvType::UPDATE, .frame = 1, .objId = 2, .parentId = 1},
        {.type = EvType::UPDATE, .frame = 1, .objId = 3},
        {.type = EvType::UPDATE, .frame = 1, .objId = 4, .parentId = 3},
        {.type = EvType::UPDATE, .frame = 1, .objId = 5, .parentId = 4},

        {.type = EvType::DRAW, .frame = 1, .objId = 1},
        {.type = EvType::DRAW, .frame = 1, .objId = 2, .parentId = 1},
        {.type = EvType::DRAW, .frame = 1, .objId = 3},
        {.type = EvType::DRAW, .frame = 1, .objId = 4, .parentId = 3},
        {.type = EvType::DRAW, .frame = 1, .objId = 5, .parentId = 4},
      });
    }},
    {"Init Nested/Disabled", 3, []{ return User::currFrame < 2; },
    []{
      return compareEvents(DATA_TEST_NESTED);
    }},

    {"Init Nested/Disabled (deferring)", 3, []{ 
      auto obj = SceneManager::getCurrent().getObjectById(6);
      // calling multiple times should only use last state
      obj->setEnabled(true);
      obj->setEnabled(false);
      obj->setEnabled(true);

      return User::currFrame < 2; 
    },
    []{
      return compareEvents(DATA_TEST_NESTED);
    }},

    {"Disable Leaf", 4, []{ 
      if(User::currFrame == 2) {
        auto obj = SceneManager::getCurrent().getObjectById(6);
        obj->setEnabled(false);
      }
      return User::currFrame < 3;
    },
    []{
      auto data = DATA_TEST_NESTED;
      data.push_back({.type = EvType::EV_DISABLE, .frame = 2, .objId = 6, .parentId = 1, .enabled = false});

      data.push_back({.type = EvType::UPDATE, .frame = 2, .objId = 1});
      data.push_back({.type = EvType::DRAW, .frame = 2, .objId = 1});
      return compareEvents(data);
    }},

    {"Disable Leaf (deferring)", 4, []{ 
      if(User::currFrame == 2) {
        auto obj = SceneManager::getCurrent().getObjectById(6);
        // calling multiple times should only use last state
        obj->setEnabled(true);
        obj->setEnabled(true);
        obj->setEnabled(false);
        obj->setEnabled(false);
        obj->setEnabled(true);
        obj->setEnabled(false);
      }
      return User::currFrame < 3;
    },
    []{
      auto data = DATA_TEST_NESTED;
      EvCapture obj1 = {.frame = 2, .objId = 1};
      EvCapture obj6 = {.frame = 2, .objId = 6, .parentId = 1};

      data.push_back(obj6.event(EvType::EV_DISABLE).disabled());

      data.push_back(obj1.event(EvType::UPDATE));
      data.push_back(obj1.event(EvType::DRAW));
      return compareEvents(data);
    }},

    {"Disable Nested", 4, []{ 
      if(User::currFrame == 2) {
        auto obj = SceneManager::getCurrent().getObjectById(1);
        obj->setEnabled(false);
      }
      return User::currFrame < 5; // should no longer do anything
    },
    []{
      auto data = DATA_TEST_NESTED;
      data.push_back({.type = EvType::EV_DISABLE, .frame = 2, .objId = 1,                .enabled = false});
      data.push_back({.type = EvType::EV_DISABLE, .frame = 2, .objId = 6, .parentId = 1, .enabled = false});

      return compareEvents(data);
    }},

    {"Disable/Enable Leaf", 4, []{ 
      auto obj = SceneManager::getCurrent().getObjectById(6);
      if(User::currFrame == 2)obj->setEnabled(false);
      if(User::currFrame == 3)obj->setEnabled(true);
      return User::currFrame < 4;
    },
    []{
      auto data = DATA_TEST_NESTED;
      EvCapture obj1 = {.frame = 2, .objId = 1};
      EvCapture obj6 = {.frame = 2, .objId = 6, .parentId = 1};
      data.push_back(obj6.event(EvType::EV_DISABLE).disabled());

      data.push_back(obj1.event(EvType::UPDATE));
      data.push_back(obj1.event(EvType::DRAW));

      ++obj1.frame; ++obj6.frame;

      data.push_back(obj6.event(EvType::EV_ENABLE));

      data.push_back(obj1.event(EvType::UPDATE));
      data.push_back(obj6.event(EvType::UPDATE));

      data.push_back(obj1.event(EvType::DRAW));
      data.push_back(obj6.event(EvType::DRAW));

      return compareEvents(data);
    }},

    {"Disable/Enable Nested", 4, []{ 
      auto obj = SceneManager::getCurrent().getObjectById(1);
      if(User::currFrame == 2)obj->setEnabled(false);
      if(User::currFrame == 3)obj->setEnabled(true);
      return User::currFrame < 4;
    },
    []{
      auto data = DATA_TEST_NESTED;
      EvCapture obj1 = {.frame = 2, .objId = 1};
      EvCapture obj6 = {.frame = 2, .objId = 6, .parentId = 1};

      data.push_back(obj1.event(EvType::EV_DISABLE).disabled());
      data.push_back(obj6.event(EvType::EV_DISABLE).disabled());

      ++obj1.frame; ++obj6.frame;

      data.push_back(obj1.event(EvType::EV_ENABLE));
      data.push_back(obj6.event(EvType::EV_ENABLE));

      data.push_back(obj1.event(EvType::UPDATE));
      data.push_back(obj6.event(EvType::UPDATE));

      data.push_back(obj1.event(EvType::DRAW));
      data.push_back(obj6.event(EvType::DRAW));

      return compareEvents(data);
    }},

    {"Disable Root - Toggle Leaf", 4, []{ 
      auto obj1 = SceneManager::getCurrent().getObjectById(1);
      auto obj6 = SceneManager::getCurrent().getObjectById(6);

      if(User::currFrame == 2)obj1->setEnabled(false);
        if(User::currFrame == 3)obj6->setEnabled(false);
        if(User::currFrame == 4)obj6->setEnabled(true);
      if(User::currFrame == 5)obj1->setEnabled(true);

      return User::currFrame < 6;
    },
    []{
      auto data = DATA_TEST_NESTED;
      EvCapture obj1 = {.frame = 2, .objId = 1};
      EvCapture obj6 = {.frame = 2, .objId = 6, .parentId = 1};

      data.push_back(obj1.event(EvType::EV_DISABLE).disabled());
      data.push_back(obj6.event(EvType::EV_DISABLE).disabled());

      obj1.frame = obj6.frame = 3;
      // <Nothing>
      obj1.frame = obj6.frame = 4;
      // <Nothing>
      obj1.frame = obj6.frame = 5;

      data.push_back(obj1.event(EvType::EV_ENABLE));
      data.push_back(obj6.event(EvType::EV_ENABLE));

      data.push_back(obj1.event(EvType::UPDATE));
      data.push_back(obj6.event(EvType::UPDATE));

      data.push_back(obj1.event(EvType::DRAW));
      data.push_back(obj6.event(EvType::DRAW));

      return compareEvents(data);
    }},

    {"Enable Nested", 4, []{ 
      auto obj3 = SceneManager::getCurrent().getObjectById(3);
      auto obj5 = SceneManager::getCurrent().getObjectById(5);
      if(User::currFrame == 2)obj3->setEnabled(true);
      if(User::currFrame == 3)obj5->setEnabled(true);
      return User::currFrame < 4;
    },
    []{
      auto data = DATA_TEST_NESTED;
      EvCapture obj1 = {.frame = 2, .objId = 1};
        EvCapture obj6 = {.frame = 2, .objId = 6, .parentId = 1};

      EvCapture obj3 = {.frame = 2, .objId = 3};
        EvCapture obj4 = {.frame = 2, .objId = 4, .parentId = 3};
          EvCapture obj5 = {.frame = 2, .objId = 5, .parentId = 4};

      data.push_back(obj3.event(EvType::EV_ENABLE));
      data.push_back(obj4.event(EvType::EV_ENABLE)); // due to parent

      data.push_back(obj1.event(EvType::UPDATE));
      data.push_back(obj6.event(EvType::UPDATE));
      data.push_back(obj3.event(EvType::UPDATE));
      data.push_back(obj4.event(EvType::UPDATE));

      data.push_back(obj1.event(EvType::DRAW));
      data.push_back(obj6.event(EvType::DRAW));
      data.push_back(obj3.event(EvType::DRAW));
      data.push_back(obj4.event(EvType::DRAW));

      obj1.frame = obj6.frame = obj3.frame = obj4.frame = obj5.frame = 3;
      data.push_back(obj5.event(EvType::EV_ENABLE));

      data.push_back(obj1.event(EvType::UPDATE));
      data.push_back(obj6.event(EvType::UPDATE));
      data.push_back(obj3.event(EvType::UPDATE));
      data.push_back(obj4.event(EvType::UPDATE));
      data.push_back(obj5.event(EvType::UPDATE));

      data.push_back(obj1.event(EvType::DRAW));
      data.push_back(obj6.event(EvType::DRAW));
      data.push_back(obj3.event(EvType::DRAW));
      data.push_back(obj4.event(EvType::DRAW));
      data.push_back(obj5.event(EvType::DRAW));


      return compareEvents(data);
    }},

    {"Delete Flat", 1, []{ 
      if(User::currFrame == 2) {
        auto obj = SceneManager::getCurrent().getObjectById(2);
        obj->remove();
      }
      return User::currFrame < 3; 
    },
    []{
      return compareEvents({
        // Scene load
        {.type = EvType::INIT, .frame = 0, .objId = 1},
        {.type = EvType::INIT, .frame = 0, .objId = 2},
        {.type = EvType::INIT, .frame = 0, .objId = 3},

        // first frame
        {.type = EvType::EV_READY, .frame = 1, .objId = 1},
        {.type = EvType::EV_READY, .frame = 1, .objId = 2},
        {.type = EvType::EV_READY, .frame = 1, .objId = 3},

        {.type = EvType::UPDATE,   .frame = 1, .objId = 1},
        {.type = EvType::UPDATE,   .frame = 1, .objId = 2},
        {.type = EvType::UPDATE,   .frame = 1, .objId = 3},

        {.type = EvType::DRAW,     .frame = 1, .objId = 1},
        {.type = EvType::DRAW,     .frame = 1, .objId = 2},
        {.type = EvType::DRAW,     .frame = 1, .objId = 3},

        // second frame
        {.type = EvType::UPDATE, .frame = 2, .objId = 1},
        {.type = EvType::UPDATE, .frame = 2, .objId = 3},

        {.type = EvType::DESTROY, .frame = 2, .objId = 2, .enabled = false},

        {.type = EvType::DRAW, .frame = 2, .objId = 1},
        {.type = EvType::DRAW, .frame = 2, .objId = 3},
      });
    }},
    {"Delete Nested", 3, []{ 
      if(User::currFrame == 2) {
        auto obj = SceneManager::getCurrent().getObjectById(3);
        obj->remove();
      }
      return User::currFrame < 3; 
    },
    []{
      auto data = DATA_TEST_NESTED;

      data.push_back({.type = EvType::UPDATE, .frame = 2, .objId = 1});
      data.push_back({.type = EvType::UPDATE, .frame = 2, .objId = 6, .parentId = 1});

      data.push_back({.type = EvType::DESTROY, .frame = 2, .objId = 3,                .enabled = false});
      data.push_back({.type = EvType::DESTROY, .frame = 2, .objId = 4, .parentId = 3, .enabled = false});
      data.push_back({.type = EvType::DESTROY, .frame = 2, .objId = 5, .parentId = 4, .enabled = false});

      data.push_back({.type = EvType::DRAW, .frame = 2, .objId = 1});
      data.push_back({.type = EvType::DRAW, .frame = 2, .objId = 6, .parentId = 1});

      return compareEvents(data);
    }},
   {"Delete+Disable in same frame", 3, []{ 
      auto obj = SceneManager::getCurrent().getObjectById(6);
      if(User::currFrame == 2) {
        obj->setEnabled(false);
        obj->remove();
      }
      return User::currFrame < 4; 
    },
    []{
      auto data = DATA_TEST_NESTED;

      EvCapture obj1 = {.frame = 2, .objId = 1};
      EvCapture obj6 = {.frame = 2, .objId = 6, .parentId = 1};

      data.push_back(obj1.event(EvType::UPDATE));
      data.push_back(obj6.event(EvType::DESTROY).disabled()); // no disable event, directly destroyed
      data.push_back(obj1.event(EvType::DRAW));

      obj1.frame = obj6.frame = 3;

      data.push_back(obj1.event(EvType::UPDATE));
      data.push_back(obj1.event(EvType::DRAW));

      return compareEvents(data);
    }},
    {"Delete+Enable in same frame", 3, []{ 
      auto obj = SceneManager::getCurrent().getObjectById(2);
      if(User::currFrame == 2) {
        obj->setEnabled(true);
        obj->remove();
      }
      return User::currFrame < 4; 
    },
    []{
      auto data = DATA_TEST_NESTED;

      EvCapture obj1 = {.frame = 2, .objId = 1};
      EvCapture obj2 = {.frame = 2, .objId = 2, .parentId = 1};
      EvCapture obj6 = {.frame = 2, .objId = 6, .parentId = 1};

      data.push_back(obj1.event(EvType::UPDATE));
      data.push_back(obj6.event(EvType::UPDATE));
      data.push_back(obj2.event(EvType::DESTROY).disabled()); // no enable event, directly destroyed
      data.push_back(obj1.event(EvType::DRAW));
      data.push_back(obj6.event(EvType::DRAW));

      obj1.frame = obj6.frame = 3;

      data.push_back(obj1.event(EvType::UPDATE));
      data.push_back(obj6.event(EvType::UPDATE));
      
      data.push_back(obj1.event(EvType::DRAW));
      data.push_back(obj6.event(EvType::DRAW));

      return compareEvents(data);
    }},
  };

  void onGameInit()
  {
    testResult = {};
    testResult.resize(TESTS.size(), -1);
    currTest = 0;
  }

  void onScenePreLoad()
  {
    User::currFrame = 0;
    User::events = {};
  }

  void onSceneUpdate()
  {
    if(testsDone) {
      onScenePreLoad(); // reset each frame once done
      return;
    }

    const auto &TEST = TESTS[currTest];

    ++User::currFrame;
    if(TEST.fnUpdate())return;

    debugf("==== Run Test %ld: %s ====\n\n", currTest, TEST.name);
    testResult[currTest] = TEST.fnTest() ? 1 : 0;

    ++currTest;
    if(currTest < TESTS.size()) {
      auto nextSceneId = TESTS[currTest].sceneId;
      if(nextSceneId != SceneManager::getCurrent().getId()) {
        SceneManager::load(TESTS[currTest].sceneId);
      } else {
        SceneManager::reload();
      }
      
    } else {
      testsDone = true;
      debugf("Tests done!\n");
    }
  }

  void onSceneDraw2D()
  {
    Debug::printStart();

    int16_t posY = 16;
    Debug::print(22, posY, "Tests - (Object State)");
    posY += 16;

    for(uint32_t t=0; t<testResult.size(); ++t) 
    {
      auto res = testResult[t];
      Debug::printf(22, posY, "%02d | %s : ", t, TESTS[t].name);
      uint16_t posX = 268;

      if(res < 0) {
        Debug::printf(posX, posY, "Pending...");
      }
      if(res == 0) {
        Debug::setColor({0xFF, 0x22, 0x22, 0xFF});
        Debug::printf(posX, posY, "FAIL!");
        Debug::setColor();
      }
      if(res == 1) {
        Debug::setColor({0x22, 0xFF, 0x22, 0xFF});
        Debug::printf(posX, posY, "OK");
        Debug::setColor();
      }

      posY += 9;
    }

    Debug::isMonospace = false;
  }
}
