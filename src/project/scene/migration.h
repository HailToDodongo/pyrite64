/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Project
{
  class Project;
}

/**
 * Versioned upgrade of scene and prefab files.
 *
 * Every file carries a "version" field. A file older than FILE_VERSION is brought up to date by
 * running each step whose target version is newer than the file's, in order, so a project can
 * skip any number of releases and still upgrade in one go.
 *
 * Everything a step knows lives in migration.cpp, including which component properties it has to
 * touch: component values are converted through the component's own serialize/deserialize and
 * addressed by property name, so no component needs to define migration code.
 *
 * Adding a migration:
 *   1. write a `void stepToVn(Context&)` in migration.cpp
 *   2. add `{n, "what changes, in user terms", stepToVn}` to STEPS, next to the step's own code
 *   3. bump FILE_VERSION
 * Scanning, chaining, the confirmation dialog and saving are generic and need no changes.
 * A step that also has to touch something outside the documents adds a `runProject` to the
 * same entry.
 *
 * Adding a kind of file that can be migrated (node graphs, asset configs, ...) is a DocType
 * value plus one DOC_KINDS entry in migration.cpp saying how to find, load and save it. A step
 * is handed every kind that is behind its version and converts the ones it cares about.
 *
 * Consent for migration is asked for exactly once when a project is opened (see Editor::MigrationOverlay). 
 * A build of an outdated project (CLI) fails with a message prompting the user to open and migrate first.
 */
namespace Project::Migration
{
  /// Version written to newly saved scenes and prefabs. Bump when appending a step.
  constexpr int FILE_VERSION = 2;

  /// Kinds of file that can be migrated. Adding one only touches migration.cpp, see DOC_KINDS.
  enum class DocType
  {
    SCENE,
    PREFAB,

    _SIZE
  };

  /// A file that is behind FILE_VERSION.
  struct PendingDoc
  {
    DocType type{DocType::SCENE};
    std::string name{};
    std::string path{};
    int version{1};
    int id{0}; // Kind-specific handle. Scenes put their scene id here since they are not addressed by path.
  };

  struct ScanResult
  {
    std::vector<PendingDoc> docs{};
    /// Summaries of every step that will run, deduplicated across all documents.
    std::vector<const char*> summaries{};

    bool empty() const { return docs.empty(); }
    size_t countOf(DocType type) const;
  };

  /**
   * Names what a scan found, for messages shown to the user: "2 prefab(s) and 1 scene(s)".
   * Lists only the kinds that turned up and learns about new ones on its own, so no caller has
   * to know which kinds exist.
   */
  std::string describe(const ScanResult &scan);

  /**
   * Lists every document of the project that is behind FILE_VERSION.
   * Reads only document versions, so it is cheap enough to call on every project open and build.
   */
  ScanResult scanProject(Project &project);

  /**
   * Converts and re-saves every document in `scan`.
   * Rewrites project files in place, so only call after the user agreed.
   */
  void apply(Project &project, const ScanResult &scan);
}
