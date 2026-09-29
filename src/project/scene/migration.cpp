/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "migration.h"

#include <algorithm>
#include <filesystem>

#include "object.h"
#include "prefab.h"
#include "scene.h"
#include "sceneManager.h"
#include "../project.h"
#include "../assetManager.h"
#include "../../utils/fs.h"
#include "../../utils/hash.h"
#include "../../utils/json.h"
#include "../../utils/logger.h"
#include "../../utils/prop.h"

namespace fs = std::filesystem;

namespace
{
  using namespace Project;

  /**
   * The document a step converts, already loaded by its DocKind.
   *
   * The payload is a set of optional pointers rather than one type, because the kinds have
   * nothing in common: only the ones that fit `docType` are non-null, and a kind that carries
   * something else (a raw json document, an asset config) adds its own field here.
   */
  struct Context
  {
    AssetManager &assets;
    Migration::DocType docType{Migration::DocType::SCENE};
    /// SCENE: the container whose children are the real objects. PREFAB: the object itself.
    Object *root{nullptr};
    /// SCENE only.
    SceneConf *conf{nullptr};
  };

  struct Step
  {
    int toVersion{};
    /// One line shown to the user before the migration runs.
    const char *summary{};
    /// Converts one document. Called for every document of every kind that is behind this
    /// version, so it has to ignore the kinds it does not touch.
    void (*run)(Context &ctx){};
    /// Optional, for changes that live outside the documents (asset configs, build outputs).
    /// Runs once per migration, after every document has been converted.
    void (*runProject)(::Project::Project &project){};
  };

  void stepToV2(Context &ctx);
  void projectStepToV2(::Project::Project &project);


  /// All known migration steps, ordered by target version.
  /// Add a new one here and bump Migration::FILE_VERSION to make it run on older files.
  const Step STEPS[] = {
    {2, "Positions, sizes and distances are converted from visual units to meters,"
        " 3D models are rebuilt at automatic vertex precision", stepToV2, projectStepToV2}
  };

  /// Runs every step newer than `fromVersion` on one document, returns the version it is at after.
  int runSteps(int fromVersion, Context &ctx)
  {
    int version = fromVersion;
    for(const auto &step : STEPS) {
      if(step.toVersion <= version)continue;
      step.run(ctx);
      version = step.toVersion;
    }
    return std::max(version, fromVersion);
  }

  // =======================================================================================
  // v2: visual units -> meters
  //
  // Pre-v2 files stored every length in "visual units" (`visualUnitsPerMeter` of them per
  // meter) and rendered models at the size their vertices were quantized to (the asset's
  // `baseScale`). Meters are the unit now, and the vertex scale is divided out at render time.
  // =======================================================================================

  /// Assumed visual units per meter for a pre-v2 document that carries no scene config.
  constexpr float V1_UNITS_PER_METER = 100.0f;

  /// What a length is measured against, which decides what a pre-v2 value has to be divided by.
  enum class LenKind
  {
    /// used as-is by the runtime (camera planes, light radius)
    ABSOLUTE,
    /// multiplied by the object's world scale (collider/culling extents)
    RELATIVE,
  };

  struct LengthProp
  {
    int compId{};
    const char *name{};
    LenKind kind{};
  };

  /**
   * Every component property this step converts, addressed by component id and property name.
   * This table is the only place that knows about them: values are read and written through the
   * component's own serialize/deserialize, and override slots through the key the runtime
   * resolves, which is derived from the property name alone. Components stay free of any
   * migration code.
   */
  constexpr LengthProp V2_LENGTHS[] = {
    {2, "size",       LenKind::ABSOLUTE}, // Light: point-light radius
    {3, "near",       LenKind::ABSOLUTE}, // Camera
    {3, "far",        LenKind::ABSOLUTE},
    {3, "orthoSize",  LenKind::ABSOLUTE},
    {5, "halfExtend", LenKind::RELATIVE}, // Collider
    {5, "offset",     LenKind::RELATIVE},
    {8, "halfExtend", LenKind::RELATIVE}, // Culling
    {8, "offset",     LenKind::RELATIVE},
  };

  /// Conversion state for one object while migrating a pre-meter file.
  struct V1Context
  {
    // visual units per meter of the file being migrated
    float visualUnitsPerMeter{V1_UNITS_PER_METER};
    // divisor for lengths the runtime multiplies by the object's world scale
    float relativeDiv{V1_UNITS_PER_METER};
    // false while walking a prefab definition reached through an instance: those values belong
    // to the prefab file and are migrated when it is loaded, only overrides stored on the
    // enclosing instances are patched here.
    bool patchValues{true};
    // number of leading PropScope layers that belong to migratable (non-definition) maps
    size_t patchableLayers{0};
    // override map the runtime reads bare (un-scoped) keys from for this object's components,
    // i.e. Property::resolve's fallback map. Null while the fallback would land on a map that
    // belongs to a prefab definition rather than the file being migrated.
    std::unordered_map<uint64_t, GenericValue> *ownOverrides{nullptr};

    float factorFor(LenKind kind) const {
      return 1.0f / (kind == LenKind::ABSOLUTE ? visualUnitsPerMeter : relativeDiv);
    }
  };

  /// Scales a stored override value. A length is only ever a distance or a 3D extent.
  void scaleValue(GenericValue &val, float factor)
  {
    switch(val.type) {
      case GenericValue::typeToId<float>():     val.valFloat *= factor; break;
      case GenericValue::typeToId<glm::vec3>(): val.valVec3  *= factor; break;
      default: break;
    }
  }

  /// Scales the same length in a component's serialized JSON, scalar or vector alike.
  void scaleJson(nlohmann::json &val, float factor)
  {
    if(val.is_number()) {
      val = val.get<float>() * factor;
    } else if(val.is_array()) {
      for(auto &item : val) {
        if(item.is_number())item = item.get<float>() * factor;
      }
    }
  }

  /**
   * Scales every override slot that resolves to `propId`: the one on the object being converted
   * plus the one each enclosing prefab instance may hold for it.
   */
  void patchOverrides(uint64_t propId, float factor, const V1Context &ctx)
  {
    auto patchKey = [factor](std::unordered_map<uint64_t, GenericValue> *map, uint64_t key) {
      auto it = map->find(key);
      if(it != map->end())scaleValue(it->second, factor);
    };

    size_t layerCount = std::min(ctx.patchableLayers, PropScope::stack.size());
    bool bareIsScoped = false;
    for(size_t i = 0; i < layerCount; ++i) {
      auto &layer = PropScope::stack[i];
      auto *map = const_cast<std::unordered_map<uint64_t, GenericValue>*>(layer.overrides);
      patchKey(map, PropScope::combine(layer.pathHash, propId));
      if(map == ctx.ownOverrides && layer.pathHash == 0)bareIsScoped = true;
    }

    // Property::resolve falls back to the bare key on the owning object's own map, which is how
    // overrides authored before keys were path-scoped still apply. They have to be converted too.
    // Object-level props are already reached that way above (their path hash is 0), so those are
    // skipped here rather than scaled twice.
    if(ctx.ownOverrides && !bareIsScoped)patchKey(ctx.ownOverrides, propId);
  }

  /// Converts an object's own property (pos/scale), value and overrides alike.
  template<typename T>
  void patchProp(Property<T> &prop, float factor, const V1Context &ctx)
  {
    if(ctx.patchValues)prop.value = prop.value * factor;
    patchOverrides(prop.id, factor, ctx);
  }

  /**
   * Converts the lengths of one component, if it has any in V2_LENGTHS.
   * The values go through the component's own serializer so this stays generic, the override
   * slots through the property name, which is what the key is hashed from.
   */
  void migrateCompToV2(Component::Entry &entry, const V1Context &ctx)
  {
    bool hasLengths = false;
    for(const auto &len : V2_LENGTHS)hasLengths |= (len.compId == entry.id);
    if(!hasLengths)return;

    const auto &info = Component::TABLE[entry.id];
    if(!info.funcSerialize || !info.funcDeserialize)return;

    PropScope::Path compPath(entry.uuid);

    nlohmann::json data{};
    if(ctx.patchValues)data = info.funcSerialize(entry);

    for(const auto &len : V2_LENGTHS)
    {
      if(len.compId != entry.id)continue;
      float factor = ctx.factorFor(len.kind);
      if(ctx.patchValues && data.contains(len.name))scaleJson(data[len.name], factor);
      patchOverrides(Utils::Hash::crc64(len.name), factor, ctx);
    }

    if(ctx.patchValues)entry.data = info.funcDeserialize(data);
  }

  /**
   * Vertex scale of the 3D model an object shows, as it was configured before v2.
   * Model wins over an animated model, which wins over a collision mesh, so the visual size is
   * what is preserved exactly when an object mixes assets of differing scales.
   */
  float findLegacyBaseScale(Object &obj, Object &srcObj, AssetManager &assets)
  {
    float best = 0.0f;
    int bestPrio = 0;

    auto scan = [&](Object &source) {
      for(auto &entry : source.components) {
        int prio = 0;
        switch(entry.id) {
          case 1:  prio = 3; break; // Model
          case 10: prio = 2; break; // AnimModel
          case 4:  prio = 1; break; // CollMesh
          default: continue;
        }
        if(prio <= bestPrio)continue;

        const auto &info = Component::TABLE[entry.id];
        if(!info.funcGetModelUUID)continue;

        auto *asset = assets.getEntryByUUID(info.funcGetModelUUID(entry));
        if(!asset || asset->type != FileType::MODEL_3D || asset->conf.baseScale <= 0)continue;

        best = (float)asset->conf.baseScale;
        bestPrio = prio;
      }
    };

    scan(srcObj);
    if(&srcObj != &obj)scan(obj);
    return best;
  }

  /**
   * @param obj object to convert
   * @param ancestorScale product of the scale factors already applied at or above this object's
   *        parent, or 1 if this object's transform is not composed with its parent's. Where
   *        composition does happen, a child's local offset is scaled by its ancestors' world
   *        scale, so rescaling a parent shrinks every descendant's offset by the same amount
   *        and the offsets have to compensate for it.
   * @param expanding mirrors the flag of the same name in Build::writeObject: only inside a
   *        prefab definition are transforms composed down the tree. Scene objects (including
   *        children a scene adds to an instance) are written with their raw local values and an
   *        identity parent transform, so their transforms are absolute and must stay untouched
   *        by what happened to the object they hang under.
   */
  void migrateObjectToV2(Object &obj, AssetManager &assets, V1Context ctx,
                         float ancestorScale, bool expanding)
  {
    if(PropScope::stack.size() > PropScope::MAX_DEPTH)return;

    auto *srcObj = &obj;
    if(obj.isPrefabInstance()) {
      auto prefab = assets.getPrefabByUUID(obj.uuidPrefab.value);
      if(prefab)srcObj = &prefab->obj;
    }
    // A definition was actually resolved. If the prefab is missing, `obj` is treated as a plain
    // object so its own children still get converted.
    const bool hasDefinition = srcObj != &obj;

    // Mirrors Build::writeObject: this node's overrides stay active for its whole subtree.
    PropScope::PrefabLayer objLayer{obj.propOverrides};
    ctx.ownOverrides = nullptr;
    if(ctx.patchValues) {
      ctx.patchableLayers = PropScope::stack.size();
      // Build::writeObject hands this node to every component build, so its map is the one
      // un-scoped overrides resolve against, for the definition's components too.
      ctx.ownOverrides = &obj.propOverrides;
    }

    // Showing a model folds the model's vertex scale into the object scale, since the runtime
    // no longer renders vertex units. Inside a prefab the factor belongs to the whole chain down
    // to the mesh, not to each model-bearing object: `Build::writeObject` multiplies a composed
    // object's scale by all of its ancestors', so only the part they have not already
    // contributed is applied here. A model under an already-converted parent keeps its scale,
    // while an absolute object (ancestorScale 1) always takes the full factor.
    float baseScale = findLegacyBaseScale(obj, *srcObj, assets);
    float ownScale = 1.0f;
    if(baseScale > 0.0f && ancestorScale > 0.0f) {
      ownScale = (baseScale / ctx.visualUnitsPerMeter) / ancestorScale;
    }

    // Component lengths the runtime multiplies by the object's world scale have to account for
    // every factor applied at this object and above it.
    ctx.relativeDiv = ctx.visualUnitsPerMeter * ancestorScale * ownScale;

    patchProp(obj.pos, 1.0f / (ctx.visualUnitsPerMeter * ancestorScale), ctx);
    if(ownScale != 1.0f) {
      patchProp(obj.scale, ownScale, ctx);
    }

    auto migrateComps = [&](Object &source, bool patchValues) {
      V1Context compCtx = ctx;
      compCtx.patchValues = patchValues;
      for(auto &entry : source.components) {
        migrateCompToV2(entry, compCtx);
      }
    };
    // The definition's own values belong to the prefab file and are converted when that file is
    // migrated, only the override slots the enclosing instances hold for them are patched here.
    migrateComps(*srcObj, hasDefinition ? false : ctx.patchValues);
    if(hasDefinition)migrateComps(obj, ctx.patchValues);

    const float composedScale = ancestorScale * ownScale;

    // Resolving an instance composes its definition's subtree onto it, so from here down the
    // tree is composed even if this object itself was absolute.
    const bool childExpanding = expanding || hasDefinition;

    V1Context childCtx = ctx;
    if(hasDefinition)childCtx.patchValues = false;

    for(const auto &child : srcObj->children) {
      PropScope::Path childPath(child->uuid);
      migrateObjectToV2(*child, assets, childCtx, childExpanding ? composedScale : 1.0f,
                        childExpanding);
    }

    // Children added directly to an instance are ordinary objects of the file being migrated and
    // resolve against their own overrides only. They keep this object's `expanding`: a scene
    // hangs them off the instance without composing, a prefab composes them like any other child.
    if(hasDefinition && !obj.children.empty()) {
      PropScope::ResetScope freshScope;
      V1Context ownCtx = ctx;
      ownCtx.patchValues = true;
      ownCtx.patchableLayers = 0;
      for(const auto &child : obj.children) {
        migrateObjectToV2(*child, assets, ownCtx, expanding ? composedScale : 1.0f, expanding);
      }
    }
  }

  void stepToV2(Context &ctx)
  {
    // Only the kinds that hold an object tree carry positions and sizes.
    if(ctx.docType != Migration::DocType::SCENE && ctx.docType != Migration::DocType::PREFAB)return;
    if(!ctx.root)return;

    float visualUnits = V1_UNITS_PER_METER;
    if(ctx.conf && ctx.conf->renderScale.value > 0.0f) {
      visualUnits = ctx.conf->renderScale.value;
    }

    // Fog distances are view-space lengths the runtime now scales itself.
    if(ctx.conf) {
      float toMeters = 1.0f / visualUnits;
      for(auto *layers : {&ctx.conf->layers3D, &ctx.conf->layersPtx, &ctx.conf->layers2D}) {
        for(auto &layer : *layers) {
          layer.fogMin.value *= toMeters;
          layer.fogMax.value *= toMeters;
        }
      }
    }

    V1Context v1{};
    v1.visualUnitsPerMeter = visualUnits;
    v1.relativeDiv = visualUnits;

    PropScope::ResetScope freshScope;
    if(ctx.docType == Migration::DocType::SCENE) {
      // the scene root only groups the real objects, whose transforms are absolute
      for(const auto &child : ctx.root->children) {
        migrateObjectToV2(*child, ctx.assets, v1, 1.0f, false);
      }
    } else {
      // a prefab is baked flat and root-relative, so its whole tree is composed
      migrateObjectToV2(*ctx.root, ctx.assets, v1, 1.0f, true);
    }
  }

  /**
   * Vertex precision is computed from a model's bounds now instead of being read from the
   * asset's `baseScale`, so nearly every model quantizes to a different scale than the .t3dm
   * that is already sitting in `filesystem/`. The build only compares that file's timestamp
   * against the glTF, which a migration never touches, so it would happily ship the stale one
   * against the new scale in the asset table. Dropping the outputs makes it re-export them.
   */
  void projectStepToV2(::Project::Project &project)
  {
    auto projectPath = fs::path{project.getPath()};
    int cleared = 0;
    for(const auto &entry : project.getAssets().getTypeEntries(FileType::MODEL_3D)) {
      std::error_code ec{};
      if(fs::remove(projectPath / entry.outPath, ec))++cleared;
    }
    if(cleared > 0) {
      Utils::Logger::log("Cleared " + std::to_string(cleared) +
        " built 3D model(s), they are re-exported at the new vertex precision on the next build");
    }
  }

  // =======================================================================================
  // Document kinds
  //
  // One entry per kind of file that can be migrated. Everything that differs between them lives
  // here: how to find them, how to load one, and how to write it back. Steps see only the
  // loaded document, so adding a kind never touches a step, and a step that does not care about
  // a kind simply ignores it.
  // =======================================================================================

  /// Version of a document stored as plain JSON with a top-level "version" field.
  int readJsonVersion(const fs::path &path)
  {
    auto doc = nlohmann::json::parse(Utils::FS::loadTextFile(path.string()), nullptr, false);
    if(!doc.is_object())return Migration::FILE_VERSION; // unreadable, leave it alone
    return doc.value("version", 1);
  }

  void scanPrefabs(::Project::Project &project, std::vector<Migration::PendingDoc> &out)
  {
    for(const auto &entry : project.getAssets().getTypeEntries(FileType::PREFAB)) {
      int version = entry.prefab ? entry.prefab->fileVersion : readJsonVersion(entry.path);
      if(version >= Migration::FILE_VERSION)continue;

      out.push_back({
        .type = Migration::DocType::PREFAB,
        .name = entry.name,
        .path = entry.path,
        .version = version,
      });
    }
  }

  void migratePrefab(::Project::Project &project, const Migration::PendingDoc &doc)
  {
    auto &assets = project.getAssets();
    auto *entry = assets.getByPath(doc.path);
    if(!entry || !entry->prefab)return;

    // Converted in place: the loaded copy is what the editor, and every scene converted after
    // this one, resolve their instances against.
    Context ctx{
      .assets = assets,
      .docType = Migration::DocType::PREFAB,
      .root = &entry->prefab->obj,
    };
    entry->prefab->fileVersion = runSteps(doc.version, ctx);
    entry->prefab->save(doc.path);
    Utils::Logger::log("Migrated prefab '" + doc.name + "'");
  }

  void scanScenes(::Project::Project &project, std::vector<Migration::PendingDoc> &out)
  {
    auto scenesPath = fs::path{project.getPath()} / "data" / "scenes";
    if(!fs::exists(scenesPath))return;

    for(const auto &dir : fs::directory_iterator{scenesPath}) {
      if(!dir.is_directory())continue;
      auto scenePath = dir.path() / "scene.json";
      if(!fs::exists(scenePath))continue;

      int version = readJsonVersion(scenePath);
      if(version >= Migration::FILE_VERSION)continue;

      int id = 0;
      try { id = std::stoi(dir.path().filename().string()); } catch(...) { continue; }

      out.push_back({
        .type = Migration::DocType::SCENE,
        .name = "Scene " + std::to_string(id),
        .path = scenePath.string(),
        .version = version,
        .id = id,
      });
    }
  }

  void migrateScene(::Project::Project &project, const Migration::PendingDoc &doc)
  {
    // Converted on a throwaway copy: migration runs before any scene is opened, so there is
    // never a loaded one to keep in sync.
    Scene scene{doc.id, project.getPath()};
    Context ctx{
      .assets = project.getAssets(),
      .docType = Migration::DocType::SCENE,
      .root = &scene.getRootObject(),
      .conf = &scene.conf,
    };
    runSteps(doc.version, ctx);
    scene.save();
    Utils::Logger::log("Migrated " + doc.name);
  }

  /// Everything that differs between the kinds of document a migration can convert.
  struct DocKind
  {
    Migration::DocType type{};
    /// What to call it in the confirmation dialog, singular and lowercase.
    const char *name{};
    /// Appends every document of this kind that is behind FILE_VERSION.
    void (*scan)(::Project::Project &project, std::vector<Migration::PendingDoc> &out){};
    /// Loads one document, runs the steps on it via runSteps() and writes it back.
    void (*migrate)(::Project::Project &project, const Migration::PendingDoc &doc){};
  };

  /// Every kind of file that can be migrated, in dependency order: both the scan and the
  /// conversion follow this table, and a scene resolves its instances against prefabs.
  const DocKind DOC_KINDS[] = {
    {Migration::DocType::PREFAB, "prefab", scanPrefabs, migratePrefab},
    {Migration::DocType::SCENE,  "scene",  scanScenes,  migrateScene },
  };

  static_assert(std::size(DOC_KINDS) == (size_t)Migration::DocType::_SIZE,
    "every DocType needs a DOC_KINDS entry saying how to find, load and save it");

  void collectSummaries(Migration::ScanResult &scan)
  {
    int oldest = Migration::FILE_VERSION;
    for(const auto &doc : scan.docs)oldest = std::min(oldest, doc.version);

    scan.summaries.clear();
    for(const auto &step : STEPS) {
      if(step.toVersion > oldest)scan.summaries.push_back(step.summary);
    }
  }
}

namespace Project::Migration
{

size_t ScanResult::countOf(DocType type) const
{
  return std::count_if(docs.begin(), docs.end(),
    [type](const PendingDoc &d) { return d.type == type; });
}

std::string describe(const ScanResult &scan)
{
  std::vector<std::string> parts{};
  for(const auto &kind : DOC_KINDS) {
    size_t count = scan.countOf(kind.type);
    if(count > 0)parts.push_back(std::to_string(count) + " " + kind.name + "(s)");
  }

  std::string text{};
  for(size_t i = 0; i < parts.size(); ++i) {
    if(i > 0)text += (i + 1 == parts.size()) ? " and " : ", ";
    text += parts[i];
  }
  return text.empty() ? std::string{"no file"} : text;
}

ScanResult scanProject(Project &project)
{
  ScanResult scan{};
  for(const auto &kind : DOC_KINDS)kind.scan(project, scan.docs);
  collectSummaries(scan);
  return scan;
}

void apply(Project &project, const ScanResult &scan)
{
  // Kind by kind rather than in scan order, so the dependencies DOC_KINDS encodes hold even for
  // a scan result that was assembled elsewhere.
  for(const auto &kind : DOC_KINDS) {
    for(const auto &doc : scan.docs) {
      if(doc.type == kind.type)kind.migrate(project, doc);
    }
  }

  // Whatever a step has to change outside the documents. Same selection the summaries use, so
  // the user is told about exactly the steps that end up running.
  int oldest = FILE_VERSION;
  for(const auto &doc : scan.docs)oldest = std::min(oldest, doc.version);
  for(const auto &step : STEPS) {
    if(step.toVersion > oldest && step.runProject)step.runProject(project);
  }
}

}
