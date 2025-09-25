/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#pragma once
#include "project/project.h"

struct Context
{
  Project::Project *project{nullptr};
};

extern Context ctx;