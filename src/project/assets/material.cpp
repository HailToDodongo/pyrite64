/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "material.h"

#include "../../utils/json.h"
#include "../../utils/jsonBuilder.h"

namespace J = Utils::JSON;

nlohmann::json Project::Assets::MaterialTex::serialize() const
{
  auto doc = Utils::JSON::Builder{}
    .set(set)
    .set(texUUID)
    .set(placeholder)
    .set(width).set(height)
    .set(offsetS).set(offsetT)
    .set(scaleS).set(scaleT)
    .set(repeatS).set(repeatT)
    .set(mirrorS).set(mirrorT)
    .doc;

  return doc;
}

void Project::Assets::MaterialTex::deserialize(const nlohmann::json &doc)
{
  J::readProp(doc, set);
  J::readProp(doc, texUUID);
  J::readProp(doc, placeholder);
  J::readProp(doc, width);
  J::readProp(doc, height);
  J::readProp(doc, offsetS);
  J::readProp(doc, offsetT);
  J::readProp(doc, scaleS);
  J::readProp(doc, scaleT);
  J::readProp(doc, repeatS);
  J::readProp(doc, repeatT);
  J::readProp(doc, mirrorS);
  J::readProp(doc, mirrorT);
}

nlohmann::json Project::Assets::Material::serialize() const
{
  auto doc = Utils::JSON::Builder{}
    .set(isCustom)
    .set(cc).set(ccSet)
    .set(blender).set(blenderSet)
    .set(aa).set(aaSet)
    .set(fog).set(fogSet)
    .set(dither).set(ditherSet)
    .set(filter).set(filterSet)
    .set(zmode).set(zmodeSet)

    .set(zprim).set(zprimSet)
    .set(zdelta)

    .set(persp).set(perspSet)
    .set(alphaComp).set(alphaCompSet)
    .set(vertexFX)

    .set(k4).set(k4k5Set)
    .set(k5)

    .set(primLod).set(primLodSet)
    .set(primColor).set(primColorSet)
    .set(envColor).set(envColorSet)
    .doc;

  doc["tex0"] = tex0.serialize();
  doc["tex1"] = tex1.serialize();
  return doc;
}

void Project::Assets::Material::deserialize(const nlohmann::json &doc)
{
  J::readProp(doc, isCustom);
  J::readProp(doc, cc);      J::readProp(doc, ccSet);
  J::readProp(doc, blender); J::readProp(doc, blenderSet);
  J::readProp(doc, aa);      J::readProp(doc, aaSet);
  J::readProp(doc, fog);     J::readProp(doc, fogSet);
  J::readProp(doc, dither);  J::readProp(doc, ditherSet);
  J::readProp(doc, filter);  J::readProp(doc, filterSet);
  J::readProp(doc, zmode);   J::readProp(doc, zmodeSet);

  J::readProp(doc, zprim);   J::readProp(doc, zprimSet);
  J::readProp(doc, zdelta);

  J::readProp(doc, persp);
  J::readProp(doc, perspSet);
  J::readProp(doc, alphaComp);
  J::readProp(doc, alphaCompSet);

  tex0.deserialize(doc["tex0"]);
  tex1.deserialize(doc["tex1"]);

  J::readProp(doc, vertexFX);

  J::readProp(doc, k4k5Set);
  J::readProp(doc, k4);
  J::readProp(doc, k5);

  J::readProp(doc, primLod);   J::readProp(doc, primLodSet);
  J::readProp(doc, primColor); J::readProp(doc, primColorSet);
  J::readProp(doc, envColor);  J::readProp(doc, envColorSet);
}
