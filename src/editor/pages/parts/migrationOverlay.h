/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#pragma once
#include <functional>

#include "../../../project/scene/migration.h"

/**
 * Asks the user before scene/prefab files are upgraded to a newer format.
 *
 * Migration rewrites project files in place, so it never runs unattended: the caller hands over
 * what it found and what it wants to do afterwards, and only gets its callback when the user
 * agrees. On Decline, nothing is changed and the project is left as-is.
 *
 * This is the only place a migration is ever started, and it is asked for once, when a project is
 * opened (Editor::Actions::Type::PROJECT_OPEN). Declining there closes the project again.
 */
namespace Editor::MigrationOverlay
{
  /**
   * Runs `onConfirm` right away when nothing needs migrating, otherwise asks first.
   * @param scan documents that need migrating, empty to just run `onConfirm`
   * @param title what the user was trying to do, e.g. "Open Project"
   * @param onConfirm runs after the migration completed
   * @param onCancel runs when the user declined, optional
   */
  void guard(const Project::Migration::ScanResult &scan, const char *title,
             std::function<void()> onConfirm, std::function<void()> onCancel = {});

  /// Call once per frame, draws the dialog if open.
  void draw();
}
