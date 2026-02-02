/**
* @copyright 2025 - Max Bebök
* @license MIT
*/
#include "marker.h"

#include <array>
#include <libdragon.h>

namespace
{
  constinit uint32_t frameIdx{0};

  constinit uint32_t markerIdx[2]{};
  constinit std::array<P64::User::Marker::Marker, 16> markers[2]{};
}

void P64::User::Marker::init()
{

}

void P64::User::Marker::addMarker(const Marker &marker)
{
  /*
  if (markerIdx[frameIdx] < markers[frameIdx].size()) {
    markers[frameIdx][markerIdx[frameIdx]] = marker;
    markerIdx[frameIdx]++;
  }
  */
}

uint16_t P64::User::Marker::getClosestObjectId()
{
  for(uint32_t i=0; i<markerIdx[frameIdx]; ++i) {
    const auto &m = markers[frameIdx][i];
    if (m.objectId != 0) {
      return m.objectId;
    }
  }
  return 0;
}

void P64::User::Marker::nextFrame()
{
  frameIdx = (frameIdx + 1) % 2;
  markers[frameIdx].fill({});
  markerIdx[frameIdx] = 0;
}

void P64::User::Marker::draw()
{
  rdpq_set_mode_fill({0xFF, 0xFF, 0xFF, 0xFF});
  for(uint32_t i=0; i<markerIdx[frameIdx]; ++i) {
    const auto &m = markers[frameIdx][i];

    rdpq_fill_rectangle(
      m.pos[0] - m.halfSize[0],     m.pos[1] - m.halfSize[1],
      m.pos[0] + m.halfSize[0],     m.pos[1] - m.halfSize[0] + 1
    );
    rdpq_fill_rectangle(
      m.pos[0] - m.halfSize[0],     m.pos[1] + m.halfSize[1] - 1,
      m.pos[0] + m.halfSize[0],     m.pos[1] + m.halfSize[1]
    );
    rdpq_fill_rectangle(
      m.pos[0] - m.halfSize[0],     m.pos[1] - m.halfSize[1],
      m.pos[0] - m.halfSize[0] + 1, m.pos[1] + m.halfSize[1]
    );
    rdpq_fill_rectangle(
      m.pos[0] + m.halfSize[0] - 1, m.pos[1] - m.halfSize[1],
      m.pos[0] + m.halfSize[0],     m.pos[1] + m.halfSize[1]
    );
  }

}
