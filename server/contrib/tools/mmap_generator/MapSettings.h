#ifndef MMAP_GENERARTOR_MAP_SETTINGS_H
#define MMAP_GENERARTOR_MAP_SETTINGS_H

#include "Recast.h"

rcConfig get_config_for(unsigned int map);
bool should_erode_walkable_areas(unsigned int map);

#endif
