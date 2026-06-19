/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once

#include "baseNode.h"
#include "nodeSpec.h"

namespace Project::Graph::Node
{
  void refreshPinStyleColors();

  // The single concrete node type, driven entirely by a NodeSpec. All editable state
  // lives in a JSON prop bag, which is also the serialization format.
  class ScriptNode : public Base
  {
    private:
      const NodeSpec* spec{};
      nlohmann::json props{}; // property values keyed by PropDef::key (+ dynamic keys)

      void addInputPin(const PinDef &pin);
      void addOutputPin(const PinDef &pin);
      std::string evalTitleTemplate() const;

    public:
      explicit ScriptNode(const NodeSpec* spec);

      const NodeSpec* getSpec() const { return spec; }
      nlohmann::json& getProps() { return props; }

      // property accessors, read from the JSON data
      int      getInt(const std::string &key, int def = 0) const;
      uint16_t getU16(const std::string &key) const { return (uint16_t)getInt(key); }
      uint32_t getU32(const std::string &key) const { return (uint32_t)getInt(key); }
      float    getF32(const std::string &key, float def = 0.0f) const;
      bool     getBool(const std::string &key) const { return getInt(key) != 0; }
      std::string getStr(const std::string &key, const std::string &def = "") const;

      void setProp(const std::string &key, const nlohmann::json &val) { props[key] = val; }

      // Whether the input pin at 'idx' currently has a link attached.
      bool isInputConnected(uint32_t idx) const;

      // Declared prop bound to input pin 'idx' via hideIfInputConnected, or null.
      const PropDef* boundPropForInput(uint32_t idx) const;

      void refreshTitle();

      // Retype a value pin (Set/Get Var nodes): updates color and drops incompatible links.
      void setValuePinType(bool isInput, uint32_t idx, const std::string &typeId);

      // Dynamic pin helpers (used by nodes with a variable number of outputs).
      void addLogicOut(const std::string &name = "");
      void addValueOut(const std::string &name = "");

      // --- Base overrides ---
      void draw() override;
      void drawBottom() override;
      void serialize(nlohmann::json &j) override;
      void deserialize(nlohmann::json &j) override;
      void build(BuildCtx &ctx) override;
      std::string value(BuildCtx &ctx) override;
      std::vector<std::string> valueInputLiterals() override;
      void prepareBuild(BuildCtx &ctx) override { if(spec && spec->prepareBuild) spec->prepareBuild(*this, ctx); }

      std::string typeId() const override { return spec ? spec->id : std::string{}; }
      bool isEntry() const override { return spec && spec->entry; }
  };
}
