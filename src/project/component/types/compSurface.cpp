/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "../components.h"
#include "../../../context.h"
#include "../../../editor/imgui/helper.h"
#include "../../../utils/json.h"
#include "../../../utils/jsonBuilder.h"
#include "../../../utils/binaryFile.h"

namespace
{
  // values must match libdragon's 'tex_format_t'
  constexpr uint8_t FORMAT_VALUES[] = {2, 3, 14, 13, 12, 17, 16, 9, 8};
  constexpr const char* FORMAT_NAMES[] = {
    "RGBA16", "RGBA32", "IA16", "IA8", "IA4", "I8", "I4", "CI8", "CI4"
  };
  constexpr int FORMAT_COUNT = std::size(FORMAT_VALUES) - 2; // @TODO: ignore CI for now, does that even make sense?
}

namespace Project::Component::Surface
{
  struct Data
  {
    PROP_IVEC2(size);
    PROP_VEC4(clearColor);
    PROP_S32(format);    // index into FORMAT_VALUES
    PROP_S32(buffering); // buffer count - 1
    PROP_BOOL(clear);
    PROP_BOOL(depth);
  };

  std::shared_ptr<void> init(Object &obj) {
    nlohmann::json dummy = nlohmann::json::object();
    return deserialize(dummy);
  }

  nlohmann::json serialize(const Entry &entry) {
    Data &data = *static_cast<Data*>(entry.data.get());
    return Utils::JSON::Builder{}
      .set(data.size)
      .set(data.clearColor)
      .set(data.format)
      .set(data.buffering)
      .set(data.clear)
      .set(data.depth)
      .doc;
  }

  std::shared_ptr<void> deserialize(nlohmann::json &doc) {
    auto data = std::make_shared<Data>();
    Utils::JSON::readProp(doc, data->size, glm::ivec2{64, 64});
    Utils::JSON::readProp(doc, data->clearColor, glm::vec4{0.0f, 0.0f, 0.0f, 1.0f});
    Utils::JSON::readProp(doc, data->format);
    Utils::JSON::readProp(doc, data->buffering);
    Utils::JSON::readProp(doc, data->clear, true);
    Utils::JSON::readProp(doc, data->depth, false);
    return data;
  }

  void build(Object& obj, Entry &entry, Build::SceneCtx &ctx)
  {
    Data &data = *static_cast<Data*>(entry.data.get());
    auto size = glm::clamp(data.size.resolve(obj), glm::ivec2{1}, glm::ivec2{4096});
    auto format = glm::clamp(data.format.resolve(obj), 0, FORMAT_COUNT - 1);

    ctx.fileObj.write<uint16_t>(size.x);
    ctx.fileObj.write<uint16_t>(size.y);
    ctx.fileObj.writeRGBA(data.clearColor.resolve(obj));
    uint8_t flags = 0;
    if(data.clear.resolve(obj))flags |= 1 << 0;
    if(data.depth.resolve(obj))flags |= 1 << 1;

    ctx.fileObj.write<uint8_t>(FORMAT_VALUES[format]);
    ctx.fileObj.write<uint8_t>(data.buffering.resolve(obj) + 1);
    ctx.fileObj.write<uint8_t>(flags);
    ctx.fileObj.write<uint8_t>(0); // padding
  }

  void draw(Object &obj, Entry &entry)
  {
    Data &data = *static_cast<Data*>(entry.data.get());

    if (ImTable::start("Comp", &obj)) {
      ImTable::add("Name", entry.name);
      ImTable::addObjProp("Size", data.size);
      ImTable::addComboBox("Format", data.format.resolve(obj), FORMAT_NAMES, FORMAT_COUNT);
      ImTable::addComboBox("Buffering", data.buffering.resolve(obj), {
        "Single", "Double", "Triple"
      });
      ImTable::addObjProp("Clear", data.clear);
      if(data.clear.resolve(obj)) {
        ImTable::addObjProp("Clear Color", data.clearColor);
      }
      ImTable::addObjProp("Depth Buffer", data.depth);
      ImTable::end();
    }
  }
}
