/*
VMap files
You should read the map.cpp documentation first.
index:
1) Structure of a vmap file
2) How vmap files are extracted
3) WMO file format
4) WMO cluster extraction process
5) Game Object extraction process
6) dir_bin file structure
7) The Tile Assembly process
*/

// 1) Structure of a vmap file
// .vmtree files
struct vmtree_file
{
    // These files contain the structure of the BIH tree.
    char vmap_magic[8]; // "VMAPb0.4" at the time of writing
    bool tiled;         // If we have no terrain tiles, we're using global WMOs
    // BIH tree data
    char node[4];       // 'N','O','D','E'
    float lo_bound[3];  // Lower corner of bounding box
    float hi_bound[3];  // Upper corner of bounding box
    uint32_t size;      // How many nodes the tree has
    uint32_t* nodes;    // The tree is a contiguous block of uint32_t, all of them are written to the file
    uint32_t object_cnt;// How many objects we have
    uint32_t* indices;  // The indices that make up our objects are written as a contiguous block of uint32_t
    // WDT files are able to reference a global WMO for the map (for example for the AQ40 instance the entire temple is one global WMO)
    char gobj[4];       // 'G','O','B','J'
    // The files data that make up this global WMO are written to the file,
    // the written content looks exactly as the dir_bin_entry, except the
    // map_id, tile_x and tile_y are not indcluded.
};
// .vmtile files
struct vmtile_file
{
    // All models that exist in that tile (grid) is written to this file
    char vmap_magic[8]; // "VMAPb0.4" at the time of writing
    uint32_t count;     // How many models this tile contains
    /* Data that exists one for each count: */
    // Model data, this is exactly the same as for the global WMO described
    // above, in other words the dir_bin_entry, minus the map_id, tile_x and
    // tile_y.
    uint32_t node_index; // Location of node for model in tree
    /* End of data that exists one for each count*/
};
// temp_gameobject_models file
struct temp_gameobject_models_file
{
    // The following data exists many times in this file; one for each game
    // object model
    uint32_t display_id;
    uint32_t name_len;
    char* name;        // Name of m2 file (example: "Armorstand.m2" (no null-terminator))
    float lo_bound[3]; // Lower corner of bounding box
    float hi_bound[3]; // Upper corner of bounding box
};
// vmo file
struct group_data; // Part of the vmo_file
struct vmo_file
{
    // These files are more or less just writing the in-between format for WMO
    // and m2 files, so see point 4 for more information.
    // Remember that the m2 files are converted to use the same in-between
    // format.
    char vmap_magic[8]; // "VMAPb0.4" at the time of writing
    char wmod[4];       // 'W','M','O','D', denotes start of root info
    uint32_t chunk_sz;  // always 8
    uint32_t rootwmo_id;// Id of the Root WMO
    char gmod[4];       // 'G','M','O','D', denotes start of groups info
    uint32_t grp_count; // How many WMO groups there are
    group_data* grps;   // The actual group data
    char gbih[4];       // 'G','B','I','H'
    // BIH tree data -- This is the exact same type of data as written in the vmtile format
    // but it is for the individual groups
    float lo_bound[3];  // Lower corner of bounding box
    float hi_bound[3];  // Upper corner of bounding box
    uint32_t size;      // How many nodes the tree has
    uint32_t* nodes;    // The tree is a contiguous block of uint32_t, all of them are written to the file
    uint32_t object_cnt;// How many objects we have
    uint32_t* indices;  // The indices that make up our objects are written as a contiguous block of uint32_t
};
// Read below about WMO to get a better understanding of what this data is
struct group_data
{
    float lo_bound[3];  // Lower corner of bounding box
    float hi_bound[3];  // Upper corner of bounding box
    uint32_t mogp_flags;
    uint32_t group_wmo_id;
    // Vertices
    char vert[4];       // 'V','E','R','T'
    uint32_t vsize;     // 4 + sizeof(float)*3*count
    uint32_t vcount;    // how many vertices make up our model's geometry
    float* vertices;    // each group of 3 floats define one vertex
    // Triangle indices
    char trim[4];       // 'T','R','I','M'
    uint32_t tsize;     // 4 + sizeof(uint32)*3*count
    uint32_t tcount;    // How many triangles our mesh contains
    uint32_t* indices;  // these are indices into the vertices array, each group of 3 makes up one triangle that is part of our mesh
    // Mesh BIH tree
    char mbih[4];       // 'M','B','I','H'
    struct Mesh_BIH_tree // Just to avoid name collisions
    {
        float lo_bound[3];  // Lower corner of bounding box
        float hi_bound[3];  // Upper corner of bounding box
        uint32_t size;      // How many nodes the tree has
        uint32_t* nodes;    // The tree is a contiguous block of uint32_t, all of them are written to the file
        uint32_t object_cnt;// How many objects we have
        uint32_t* indices;  // The indices that make up our objects are written as a contiguous block of uint32_t
    };
    // Liquid Data (optional block)
    char liqu[4];       // 'L','I','Q','U'
    uint32 lsize;       // if 0 no liquid data exists (LIQU magic will still be present)
    uint32_t filesize;  // see WmoLiquid::GetFileSize()
    uint32_t tiles_x;   // size of liquid block
    uint32_t tiles_y;   // is made up by tiles_x and tiles_y
    float corner[3];    // top left corner, where the block is located
    uint32_t type;      // type of liquid
    float* height_map;  // height map of liquid, this will contain as many height points as the expression: (tiles_x+1)*(tiles_y+1)
    uint8_t* flags_map; // flags for each square in the liquid map, will contain as many flags as the expression: tiles_x*tiles_y
};

// 2) How vmap files are extracted
/*
Step 1: We open the .MPQ files, and search through the archive extracing each
.wmo file (described in detail further down), making up the static data of the
world.

Step 2: Just as in the map extraction we look at the WDT files and then the ADT
files and determine all the map ids, as well as figure out what models we could
not find.

Step 3: All models for game-objects are extracted, for the purpose of dynamic
LoS (described in detail further down).

Step 4: The extracted data will be read by the vmap assembler that builds the
final data that mangos will use to do line of sight calculations on (described
in detail further down).
*/

// 3) WMO file format
/*
This format has been documented previously, and for the full details please
check: http://www.pxr.dk/wowdev/wiki/index.php?title=WMO/v17
This section will only highlight what's important to the extraction process.

The WMO "file" format is actually not a format that's restricted to a single
file. Instead it's appropriate to think of WMO as a cluster of files. The WMO
cluster contains a root file that's the base of up to 512 different group
files.

The content of the type of files are described below:

=== WMO Root File ===
The WMO root file lists all the group files it contains, as well as materials,
textures, shaders, etc.
For each WMO root there can be up to 512 group files.

=== WMO Group File ===
The Group files are what makes up the actual model data of the object. An
wmo object could be something small such as a tree in the world, or something
giant such as the entire Zul'Gurub architecture. (TODO: Is this true?)

MOVI chunk: Indices to make up the triangles out of the vertices. Each group of
three indices refer to offsets into the vertex list that make up a triangle.

MOVT chunk: Vertices. 3 single floating-point numbers per vertex.

MOPY chunk: This chunk contains flags and material ID for each vertex in the
MOVT chunk. These flags say stuff as if the triangle is collidable with, etc.
*/


// 4) WMO cluster extraction process
/*
=== Extracting and converting the WMO Root ===
Step 1: When extracting a WMO cluster we begin by loading the root, this file
has a name such as:
"World\wmo\path\WMOName.wmo"
We only read the MOHD chunk of the Root data, as the rest of the data is
irrelevant to us (it's stuff like materials, textures and shaders).

Step 2: A file with the same name (excluding path and prefix data) is then
created in the Buildings directory. For example, it might be named
Zulgurub.wmo if the client's name was World\wmo\path\WMOZulgurub.wmo.

Step 3: The following data is then written to this file:
char[8]: magic string ("VMAPb04" at the time of writing). this is not from MOHD
uint32_t: this is not data from the MOHD chunk, but will be filled in later
          with a vector count we calculate.
uint32_t: this is the amount of WMO groups associated to this root (this is
          from MOHD)
uint32_t: this is the WMO id found in the second column of WMOAreaTable.dbc
          (this is from MOHD)

=== Extracting and converting the WMO Groups ===
Step 1: In order, we open the group nodes and do the below described
procedures. The name of the files are:
"World\wmo\path\WMOName_000.wmo", "World\wmo\path\WMOName_001.wmo", etc.

Step 2: The chunks we read are (you can read about what they contain on the
above linked WMO resource):
MOGP, MOPY, MOVI, MOVT, MOBA, and MLIQ

Step 3: The data that is written to the target file for each group is:
uint32_t: Flags of the MOGP chunk, for exact meaning check the WMO link above.
uint32_t: WMO group id, column 4 of WMOAreaTable.dbc
float[3]: Bounding box corner #1 
float[3]: Bounding box corner #2
uint32_t: this is named liquid flags, but all it is atm is a boolean if we have
          liquid data or not (MLIQ)
char[4]: 'G','R','P',' '
uint32_t: The size (in bytes) of the MOBA Extension that follows
uint32_t: Count of how many MOBA extensions follow.
TODO: MobaEx

Now, the file format branches depending on how the extractor was ran, if it was
passed the "-s" flag (or ran without a flag) it will not build the precise
version of the vector data, if it was given the "-l" flag, it will, however.

This means that if we're NOT building precise vector data, we look at the MOPY
chunk to determine which triangles that are flagged as ignored for collision,
we go ahead and remove them and reindex the data accordingly. Otherwise, we
write the data as is.

char[4]: 'I','N','D','X' -- marks the start of the MOVI chunk
uint32_t: size of indices block (4 + sizeof(uint16_t) * indices)
uint32_t: count of indices
uint16_t*: Array of indices. These are indices into the vertex list, grouped in
           logical groups of three to make up triangles.
char[4]: 'V','E','R','T' -- marks the start of the MOVT chunk
uint32_t: size of vertices block (4 + sizeof(float) * 3 * vertices
uint32_t: count of vertices
float*: vertices, each vertex is 3 floats.

An optional chunk of liquid data is written as well:
char[4]: 'L','I','Q','U' -- marks the start of the MLIQ chunk
uint32_t: size of liquid chunk
=== MLIQ CHUNK HEADER ===
uint32_t: number of X vertices
uint32_t: number of Y vertices
uint32_t: number of X tiles (X vertices - 1)
uint32_t: number of Y tiles (Y vertices - 1)
float[3]: Base coodrinate for X and Y
uint16_t: named "type", unsure what it is atm
float*: an array of height values of each WMO liquid vertex
uint8_t*: an array of flags, not fully sure what they do atm
*/

// 5) Game Object extraction process
/*
Step 1: We consult the GameObjectDisplayInfo.dbc to collect the names of all
gameobjects.

Step 2: We proceed to extract it depending on its extension:
".wmo" => We use the extraction process above to extract its data.
".mdl" => We skip it (at least for now).
".mdx" or ".m2" => We use the extraction process described below:

=== m2 extraction process ===
For full documentation of the M2 format, please check:
http://www.pxr.dk/wowdev/wiki/index.php?title=M2

Step 1: The .m2 header is read, and the offset to the views data.

Step 2: The views data contain offsets to the index data and vertex data, and
we proceed to load them as is.

Step 3: We then convert this .m2 file to the exact same in-between format as
described above in the WMO extraction process. The fields that do not apply are
simply zeroed out.

NOTE: The temp_gameobject_models is described in point 7, instead of here.
*/

// 6) dir_bin file structure
// This file is generated while extracting the model files, and is then used by
// the assembler. It has multiple entries in it, and each entry looks like this
struct dir_bin_entry
{
    uint32_t map_id;
    uint32_t tile_x; // remember the grids from the map.cpp documentation? tile_x is the X value for a grid in the map
    uint32_t tile_y; // this is the y value for that grid in our map. together they denote a specific grid in a map
    uint32_t flags;  // specifies type of file
    uint16_t adt_id; // Id of ADT file
    uint32_t id;     // Id of this file
    float pos[3];    // Position of model in world (X,Y,Z)
    float ori[3];    // Orientation (A,B,C)
    float scale;     // Scale of model
    // If 0x4 is set in flags, then this is a WMO model and contains a bounding box
    float upper_bound[3]; // Upper corner of bounding box
    float lower_bound[3]; // Lower corner of bounding box
    // Back to data that exists for every entry
    uint32_t len;    // Length of file name that follows
    char*    name;   // Name of model file
};

// 7) The Tile Assembly process
/*
Once all model data has been extracted (WMO for all the static "objects" in the
world, such as houses and what-not and m2 for everything else, such as game
object models) and converted into our in-between WMO format we begin the
assembly process to turn this data into data we can use for LoS calculations.

Step 1: During the extraction process before a file called dir_bin was created
and written to, this file contains the informations of the models we found. Its
layout is described above. We load this file and loop through its content. For
each dir_bin_entry in the file we create a model entry (just this data indexed
on ID), and a tile entry (the tile_x and tile_y shifted into one uint32_t, and
the dir_bin_entry.id).

The following steps will be executed for each map that has models in it:

Step 2: We proceed to calculate model bounding boxes for all M2 models as they
have none in the WDT/ADT placement data. WMO files do indeed have one, but we
need to move it to another coordinate system as it differs from the one used
for the game.

Step 3: After that we build the Bounding interval hierarchy data structure for
each map, which is a tree data structure that you can read more about here:
http://en.wikipedia.org/wiki/Bounding_interval_hierarchy
To see the build function in action you can check src/game/vmap/BIH.h and the
templated function build().

Step 4: We will then write this tree data structure named: "mapid.vmtree".
The structure of these files are described above, in point 1.

Step 5: Following that we proceed to write vmtile files for each tile (grid)
in our map, the files will be named as such: "mapid_x_y.vmtile". File layout
is described above in point 1.

Step 6: When we extracted a file called temp_gameobject_models was created
listing game objects and data about them. A file with the same name, but
different content is written to the output directory (vmaps/). This file is
going to be used by the core to load the models needed for game objects. The
structure of the file is described above in point 1.

Step 7: The in-between wmo and m2 files we had before are converted and written
to vmo files. These contain more or less the same data, see point 1 for exact
structure.
*/
