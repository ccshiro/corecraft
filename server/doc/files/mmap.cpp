/*
MMap files
You should read the map.cpp and vmap.cpp documentation first.
index:
1) Structure of an mmap file
2) How mmap files are generated
*/

struct mmap_file
{
    // This file just contains the dtNavMeshParams as is, in other words:
    struct dtNavMeshParams
    {
        float orig[3];					///< The world space origin of the navigation mesh's tile space. [(x, y, z)]
        float tileWidth;				///< The width of each tile. (Along the x-axis.)
        float tileHeight;				///< The height of each tile. (Along the z-axis.)
        int maxTiles;					///< The maximum number of tiles the navigation mesh can contain.
        int maxPolys;					///< The maximum number of polygons each tile can contain.
    };
};

struct mmtile_file
{
    // Header
    uint32 mmapMagic;       // This is a big-endian encoded integer containing the ASCII "MMAP", but since you're using little endian, your final file will contain "PAMM"
    uint32 dtVersion;       // Version of detour, 7 at the time of writing
    uint32 mmapVersion;     // MMaps version, 5 at the time of writing
    uint32 size;            // size of the content
    bool usesLiquids : 1;   // if there's liquid data
    // Data
    unsigned char* data;    // The resulting tile data, as outputted by dtCreateNavMeshData (http://www.stevefsp.org/projects/rcndoc/prod/group__detour.html#gaf56ac19e79e5948fdb1051158577e648)
};

// 2) How mmap files are generated
/*
The MMap generator is unique in the fact that it does not extract anything from
the client, instead it simply inspects the data we have extracted so far (maps
and vmaps), and generates navigation data based on that.

== Figuring out what maps and tiles (grids) to build ===
Note: the word tile is used interchangeably with grid in this discussion.

Step 1: We step into the maps/ directory, and we make a note of each map id
there is, this "note" will be used in step 3 to figure out the structure of
the map.

Step 2: We then go into the vmaps/ directory and open all .vmtree files. We add
any map id we did not find in the maps/ folder to our "note". Together they
make up all the maps we will process.

Step 3: Using all the map ids from step 1 and 2 we visit the maps/ and vmaps/
directory to figure out all tiles (grids) of a map. We take the X and Y and
pack it into one uint32_t. We now know all the grids of each map.

=== Build navmesh for each map ===
Step 1: We fill out a dtNavMeshParams with the data of our map:
(http://www.stevefsp.org/projects/rcndoc/prod/structdtNavMeshParams.html)
tileHeight: The height of a grid (533.33333)
tileWidth:  The width of a grid (533.33333)
maxTiles:   The amount of grids our map has
maxPolys:   This is set to the highest possible value of a signed integer (1 << 31)
orig:       bmin as calculated by MapBuilder::getTileBounds()

Step 2: We write this filled out dtNavMeshParams to "mmaps/mapid.mmap".

Step 3: Height and liquid data from the map is loaded using
TerrainBuilder::loadMap. Model data is loaded using TerrainBuilder::loadVMap.
Both of these functions are very nested and hard to follow.

Step 4: Unused vertices are removed from the solid data and the liquid data,
see TerrainBuilder::cleanVertices for details.

Step 5: All data is put into one big array, and offmesh input is loaded if any
was specified (see TerrainBuilder::loadOffMeshConnections).

Step 6: The recast config structure is filled out
http://www.stevefsp.org/projects/rcndoc/prod/structrcConfig.html
For settings see MapBuilder::buildMoveMapTile.

Step 7: The mmtile is written to disk.
*/