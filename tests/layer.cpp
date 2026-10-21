/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "doctest/doctest.h"

#include "project/scene/layer.h"

using namespace Project;

namespace
{
  std::vector<LayerConf> makeLayers(std::initializer_list<const char*> names) {
    std::vector<LayerConf> layers{};
    for(auto name : names) {
      LayerConf layer{};
      layer.name.value = name;
      layers.push_back(layer);
    }
    return layers;
  }
}

TEST_SUITE("Layer")
{
  TEST_CASE("keys from names are stable")
  {
    CHECK(Layer::keyFromName("3D Transp.") == Layer::keyFromName("3D Transp."));
    CHECK(Layer::keyFromName("A") != Layer::keyFromName("B"));

    static_assert(Layer::keyFromName("abcd") == 0xed82cd11); // same as the runtime's "_hash" literal
  }

  TEST_CASE("positions are valid references")
  {
    auto layers = makeLayers({"Opaque", "Transp."});
    CHECK(Layer::findIndex(layers, 1) == 1);
    CHECK(Layer::findIndex(layers, 2) == -1);
    CHECK(Layer::findIndex(layers, Layer::MAX_INDEX_REF) == -1);
  }

  TEST_CASE("finding layers by key")
  {
    auto layers = makeLayers({"Opaque", "Transp.", "Additive"});

    CHECK(Layer::findIndex(layers, Layer::REF_DEFAULT) == 0);
    CHECK(Layer::findIndex(layers, Layer::keyFromName("Transp.")) == 1);
    CHECK(Layer::findIndex(layers, Layer::keyFromName("Additive")) == 2);
    CHECK(Layer::findIndex(layers, Layer::keyFromName("Missing")) == -1);
    CHECK(Layer::findIndex({}, Layer::REF_DEFAULT) == -1);
  }

  TEST_CASE("keys survive removing and reordering layers")
  {
    auto layers = makeLayers({"Opaque", "Transp.", "Additive"});
    auto key = Layer::refFromIndex(layers, 2);

    layers.erase(layers.begin() + 1);
    CHECK(Layer::findIndex(layers, key) == 1);

    std::swap(layers[0], layers[1]);
    CHECK(Layer::findIndex(layers, key) == 0);
  }

  TEST_CASE("layers are referenced by name, invalid indices by the default reference")
  {
    auto layers = makeLayers({"Opaque", "Transp.", "layer8035639"});
    CHECK(Layer::refFromIndex(layers, 0) == Layer::keyFromName("Opaque"));
    CHECK(Layer::refFromIndex(layers, 1) == Layer::keyFromName("Transp."));
    CHECK(Layer::refFromIndex(layers, 2) == 2); // name hashes into the position range
    CHECK(Layer::refFromIndex(layers, 3) == Layer::REF_DEFAULT);
    CHECK(Layer::refFromIndex(layers, -1) == Layer::REF_DEFAULT);
  }

  TEST_CASE("names hashing into the position range are not used")
  {
    static_assert(Utils::Hash::crc32("layer8035639") == 0x0D);
    static_assert(Layer::isIndexRef(0x0D));
    CHECK_FALSE(Layer::isValidName("layer8035639"));

    auto name = Layer::makeUniqueName({}, "layer8035639");
    CHECK(name == "layer8035639 2");
    CHECK(Layer::isValidName(name));
  }

  TEST_CASE("unique names")
  {
    auto layers = makeLayers({"Layer", "Layer 2", "Other"});
    CHECK(Layer::makeUniqueName(layers, "New") == "New");
    CHECK(Layer::makeUniqueName(layers, "Layer") == "Layer 3");
    CHECK(Layer::makeUniqueName(layers, "Other", &layers[2]) == "Other");
    CHECK(Layer::makeUniqueName(layers, "Other", &layers[0]) == "Other 2");
  }
}
