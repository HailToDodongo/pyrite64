/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include <string>

namespace Editor
{

  void applyLang(const std::string &lang);
  const char* message(const std::string &id);

#define MSG_ASSET_NONE_SELECTED                   "asset.none_selected"
#define MSG_ASSET_FILE                            "asset.file"
#define MSG_ASSET_SETTINGS                        "asset.settings"
#define MSG_IMAGE_FORMAT                          "asset.image_format"
#define MSG_ASSET_COMPRESSION                     "asset.compression"
#define MSG_ASSET_COMPRESSION_DEFAULT             "asset.compression_level.default"
#define MSG_ASSET_COMPRESSION_NONE                "asset.compression_level.none"
#define MSG_ASSET_COMPRESSION_LEVEL_1             "asset.compression_level.1"
#define MSG_ASSET_COMPRESSION_LEVEL_2             "asset.compression_level.2"
#define MSG_ASSET_COMPRESSION_LEVEL_3             "asset.compression_level.3"
#define MSG_ASSET_EXCLUDE                         "asset.exclude"
#define MSG_ASSET_PREVIEW                         "asset.preview"
#define MSG_ASSET_PREVIEW_COUNT_MESHES            "asset.preview_count.meshes"
#define MSG_ASSET_PREVIEW_COUNT_TRIANGLES         "asset.preview_count.triangles"
#define MSG_ASSET_PREVIEW_COUNT_BONES             "asset.preview_count.bones"
#define MSG_ASSET_PREVIEW_COUNT_ANIMATIONS        "asset.preview_count.animations"
#define MSG_MODEL_BASE_SCALE                      "model.base_scale"
#define MSG_MODEL_CREATE_BVH                      "model.create_bvh"
#define MSG_MODEL_COLLISION                       "model.collision"
#define MSG_FONT_SIZE                             "font.size"
#define MSG_FONT_ID                               "font.id"
#define MSG_FONT_CHARSET                          "font.charset"
#define MSG_AUDIO_FORCE_MONO                      "audio.force_mono"
#define MSG_AUDIO_SAMPLE_RATE                     "audio.sample_rate"
#define MSG_AUDIO_SAMPLE_RATE_ORIGINAL            "audio.sample_rate_original"
#define MSG_OBJECT_NONE_SELECTED                  "object.none_selected"
#define MSG_OBJECT_NAME                           "object.name"
#define MSG_OBJECT_ID                             "object.id"
#define MSG_OBJECT_PREFAB                         "object.prefab"
#define MSG_OBJECT_PREFAB_BACK                    "object.prefab_back"
#define MSG_OBJECT_PREFAB_EDIT                    "object.prefab_edit"
#define MSG_OBJECT_TRANSFORM                      "object.transform"
#define MSG_OBJECT_POS                            "object.pos"
#define MSG_OBJECT_SCALE                          "object.scale"
#define MSG_OBJECT_ROT                            "object.rot"
#define MSG_OBJECT_COMPONENT_DUPLICATE            "object.component_duplicate"
#define MSG_OBJECT_COMPONENT_DELETE               "object.component_delete"
#define MSG_OBJECT_COMPONENT_ADD                  "object.component_add"
#define MSG_OBJECT_COMPONENT_COPY                 "object.component_copy"
#define MSG_OBJECT_COMPONENT_CODE                 "object.components.code"
#define MSG_OBJECT_COMPONENT_STATIC_MODEL         "object.components.static_model"
#define MSG_OBJECT_COMPONENT_LIGHT                "object.components.light"
#define MSG_OBJECT_COMPONENT_CAMERA               "object.components.camera"
#define MSG_OBJECT_COMPONENT_COLLISION_MESH       "object.components.collision_mesh"
#define MSG_OBJECT_COMPONENT_COLLISION_BODY       "object.components.collision_body"
#define MSG_OBJECT_COMPONENT_AUDIO_2D             "object.components.audio_2d"
#define MSG_OBJECT_COMPONENT_CONSTRAINT           "object.components.constraint"
#define MSG_OBJECT_COMPONENT_CULLING              "object.components.culling"
#define MSG_OBJECT_COMPONENT_NODE_GRAPH           "object.components.node_graph"
#define MSG_OBJECT_COMPONENT_ANIMATED_MODEL       "object.components.animated_model"
#define MSG_OBJECT_CAMERA_NAME                    "object.camera.name"
#define MSG_OBJECT_CAMERA_OFFSET                  "object.camera.offset"
#define MSG_OBJECT_CAMERA_SIZE                    "object.camera.size"
#define MSG_OBJECT_CAMERA_FOV                     "object.camera.fov"
#define MSG_OBJECT_CAMERA_NEAR                    "object.camera.near"
#define MSG_OBJECT_CAMERA_FAR                     "object.camera.far"
#define MSG_OBJECT_CAMERA_ASPECT                  "object.camera.aspect"
#define MSG_SCENE_ON_BOOT                         "scene.on_boot"
#define MSG_SCENE_ON_RESET                        "scene.on_reset"
  
}
