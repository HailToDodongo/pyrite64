/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "meshGen.h"
#include "../renderer/mesh.h"

void Utils::Mesh::generateCube(Renderer::Mesh&mesh) {
  mesh.vertices.clear();

  float size = 1.1f;
  for (int i=0; i<6; i++) {
    float nx = (i==0) - (i==1);
    float ny = (i==2) - (i==3);
    float nz = (i==4) - (i==5);
    float ux = (i==2 || i==3) * ((i==2) - (i==3));
    float uy = (i==4 || i==5) * ((i==4) - (i==5));
    float uz = (i==0 || i==1) * ((i==0) - (i==1));

    mesh.vertices.push_back({{ size*ux + size*nz, size*uy + size*nx, size*uz + size*ny}, {nx,ny,nz}, {1,0,0,1}, {1,0}});
    mesh.vertices.push_back({{-size*ux + size*nz, -size*uy + size*nx, -size*uz + size*ny}, {nx,ny,nz}, {0,1,0,1}, {0,1}});
    mesh.vertices.push_back({{ size*ux - size*nz, size*uy - size*nx, size*uz - size*ny}, {nx,ny,nz}, {0,0,1,1}, {1,1}});
    mesh.vertices.push_back({{ size*ux + size*nz, size*uy + size*nx, size*uz + size*ny}, {nx,ny,nz}, {1,0,0,1}, {1,0}});
    mesh.vertices.push_back({{-size*ux - size*nz, -size*uy - size*nx, -size*uz - size*ny}, {nx,ny,nz}, {1,1,0,1}, {0,0}});
    mesh.vertices.push_back({{-size*ux + size*nz, -size*uy + size*nx, -size*uz + size*ny}, {nx,ny,nz}, {0,1,0,1}, {0,1}});
  }

  mesh.vertices.clear();
  mesh.vertices.push_back({{-10,0,-10}, {0,1,0}, {1,0,0,1}, {0,0}});
  mesh.vertices.push_back({{ 10,0, 10}, {0,1,0}, {0,1,0,1}, {1,1}});
  mesh.vertices.push_back({{ 10,0,-10}, {0,1,0}, {0,0,1,1}, {1,0}});
  mesh.vertices.push_back({{-10,0,-10}, {0,1,0}, {1,0,0,1}, {0,0}});
  mesh.vertices.push_back({{-10,0, 10}, {0,1,0}, {1,1,0,1}, {0,1}});
  mesh.vertices.push_back({{ 10,0, 10}, {0,1,0}, {0,1,0,1}, {1,1}});

  mesh.vertices.push_back({{-1,-1, -1}, {0,0,-1}, {1,0,0,1}, {0,0}});
  mesh.vertices.push_back({{ 1, 1, -1}, {0,0,-1}, {0,1,0,1}, {1,1}});
  mesh.vertices.push_back({{ 1,-1, -1}, {0,0,-1}, {0,0,1,1}, {1,0}});
  mesh.vertices.push_back({{-1,-1, -1}, {0,0,-1}, {1,0,0,1}, {0,0}});
  mesh.vertices.push_back({{-1, 1, -1}, {0,0,-1}, {1,1,0,1}, {0,1}});
  mesh.vertices.push_back({{ 1, 1, -1}, {0,0,-1}, {0,1,0,1}, {1,1}});

  mesh.vertices.push_back({{-1,-1, 1}, {0,0,-1}, {1,0,0,1}, {0,0}});
  mesh.vertices.push_back({{ 1, 1, 1}, {0,0,-1}, {0,1,0,1}, {1,1}});
  mesh.vertices.push_back({{ 1,-1, 1}, {0,0,-1}, {0,0,1,1}, {1,0}});
  mesh.vertices.push_back({{-1,-1, 1}, {0,0,-1}, {1,0,0,1}, {0,0}});
  mesh.vertices.push_back({{-1, 1, 1}, {0,0,-1}, {1,1,0,1}, {0,1}});
  mesh.vertices.push_back({{ 1, 1, 1}, {0,0,-1}, {0,1,0,1}, {1,1}});
}
