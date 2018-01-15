#
# Copyright (C) 2014 Corecraft <https://www.worldofcorecraft.com/>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

file(GLOB_RECURSE source *.cpp *.h)

set(include_dirs
  ${CMAKE_SOURCE_DIR}/dep/recastnavigation/Detour
  ${CMAKE_SOURCE_DIR}/dep/recastnavigation/Recast
  ${CMAKE_SOURCE_DIR}/dep/g3dlite/include
  ${CMAKE_SOURCE_DIR}/src/framework
  ${CMAKE_SOURCE_DIR}/src/shared
  ${CMAKE_SOURCE_DIR}/src/game/
  ${CMAKE_SOURCE_DIR}/src/game/vmap
  ${CMAKE_CURRENT_SOURCE_DIR}
)

include_directories(${include_dirs})

add_executable(mmap_generator
  ${source}
)

target_link_libraries(mmap_generator
  vmap
  g3dlib
  Detour
  Recast
  shared
  framework
  ${BZIP2_LIBRARIES}
  ${ZLIB_LIBRARIES}
  ${Boost_LIBRARIES}
)

install(TARGETS mmap_generator DESTINATION ${BIN_DIR})

if(OPTIMIZED)
  add_definitions(-DOPTIMIZED_BUILD)
endif()
