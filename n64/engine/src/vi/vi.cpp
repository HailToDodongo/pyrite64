/**
* @copyright 2024 - Max Bebök
* @license MIT
*/
#include "vi.h"
#include <libdragon.h>

float P64::VI::calcRefreshRate()
{
    return vi_get_refresh_rate();
}
