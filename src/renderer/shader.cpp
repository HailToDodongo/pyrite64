/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "shader.h"

#include <cassert>
#include <stdexcept>

#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_iostream.h"
#include "SDL3_shadercross/SDL_shadercross.h"

Renderer::Shader::Shader(SDL_GPUDevice* device, const Config &conf)
  : gpuDevice{device}
{
  SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
  const SDL_GPUShaderFormat shaderCrossFormats = SDL_ShaderCross_GetSPIRVShaderFormats();
  const bool supportsNativeSpirv = (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) != 0;
  const bool supportsShaderCross = (backendFormats & shaderCrossFormats) != 0;

  if (!supportsNativeSpirv && !supportsShaderCross) {
    throw std::runtime_error("No supported shader backend for SPIR-V shader assets!");
  }

  std::string pathVert = "data/shader/" + conf.name + ".vert.spv";
  std::string pathFrag = "data/shader/" + conf.name + ".frag.spv";

  size_t vertexCodeSize;
  void* vertexCode = SDL_LoadFile(pathVert.c_str(), &vertexCodeSize);
  assert(vertexCode != nullptr);

  size_t fragmentCodeSize;
  void* fragmentCode = SDL_LoadFile(pathFrag.c_str(), &fragmentCodeSize);
  assert(fragmentCode != nullptr);

  if (supportsNativeSpirv) {
    SDL_GPUShaderCreateInfo vertexInfo{};
    vertexInfo.code = (Uint8*)vertexCode;
    vertexInfo.code_size = vertexCodeSize;
    vertexInfo.entrypoint = "main";
    vertexInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    vertexInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    vertexInfo.num_samplers = conf.vertTexCount;
    vertexInfo.num_storage_buffers = 0;
    vertexInfo.num_storage_textures = 0;
    vertexInfo.num_uniform_buffers = conf.vertUboCount;
    shaderVert = SDL_CreateGPUShader(device, &vertexInfo);
    assert(shaderVert != nullptr);

    SDL_GPUShaderCreateInfo fragmentInfo{};
    fragmentInfo.code = (Uint8*)fragmentCode;
    fragmentInfo.code_size = fragmentCodeSize;
    fragmentInfo.entrypoint = "main";
    fragmentInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    fragmentInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fragmentInfo.num_samplers = conf.fragTexCount;
    fragmentInfo.num_storage_buffers = 0;
    fragmentInfo.num_storage_textures = 0;
    fragmentInfo.num_uniform_buffers = conf.fragUboCount;
    shaderFrag = SDL_CreateGPUShader(gpuDevice, &fragmentInfo);
  } else {
    SDL_ShaderCross_SPIRV_Info vertexSpirvInfo{};
    vertexSpirvInfo.bytecode = (const Uint8*)vertexCode;
    vertexSpirvInfo.bytecode_size = vertexCodeSize;
    vertexSpirvInfo.entrypoint = "main";
    vertexSpirvInfo.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX;

    SDL_ShaderCross_GraphicsShaderResourceInfo vertexResources{};
    vertexResources.num_samplers = conf.vertTexCount;
    vertexResources.num_storage_textures = 0;
    vertexResources.num_storage_buffers = 0;
    vertexResources.num_uniform_buffers = conf.vertUboCount;

    SDL_ShaderCross_SPIRV_Info fragmentSpirvInfo{};
    fragmentSpirvInfo.bytecode = (const Uint8*)fragmentCode;
    fragmentSpirvInfo.bytecode_size = fragmentCodeSize;
    fragmentSpirvInfo.entrypoint = "main";
    fragmentSpirvInfo.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;

    SDL_ShaderCross_GraphicsShaderResourceInfo fragmentResources{};
    fragmentResources.num_samplers = conf.fragTexCount;
    fragmentResources.num_storage_textures = 0;
    fragmentResources.num_storage_buffers = 0;
    fragmentResources.num_uniform_buffers = conf.fragUboCount;

    shaderVert = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
      device, &vertexSpirvInfo, &vertexResources, 0
    );
    shaderFrag = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(
      device, &fragmentSpirvInfo, &fragmentResources, 0
    );
  }

  assert(shaderVert != nullptr);
  assert(shaderFrag != nullptr);

  SDL_free(fragmentCode);
  SDL_free(vertexCode);
}

Renderer::Shader::~Shader()
{
  SDL_ReleaseGPUShader(gpuDevice, shaderVert);
  SDL_ReleaseGPUShader(gpuDevice, shaderFrag);
}
