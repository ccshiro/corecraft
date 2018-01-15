/*
Map files
You should read the dbc.cpp documentation first, as it's closely related to
the map format.
index:
1) Structure of a map file
2) How map files are extracted
3) WoW's usage of the cartesian coordinate system
4) Structure of an ADT file
5) How ADT files are converted to map files
*/

// 1) Structure of a map file

// every map file starts with this header:
struct GridMapFileHeader
{
    uint32_t mapMagic;            // contains "MAPS"
    uint32_t versionMagic;        // contains a version number defined in the core, this changes when we make modifications to the map format
    uint32_t areaMapOffset;       // offset of GridMapAreaChunk
    uint32_t areaMapSize;         // size of GridMapAreaData (this size is static and unnecessary)
    uint32_t heightMapOffset;     // offset of height data
    uint32_t heightMapSize;       // size of height data
    uint32_t liquidMapOffset;     // offset of liquid data
    uint32_t liquidMapSize;       // size of liquid data
    uint32_t holesOffset;         // offset of holes data
    uint32_t holesSize;           // size of holes data
};

// if the offsets are 0 then that data is not contained in the map file


// area data
// ---------
// a single .map file seems to contain Area data for 1 square of 533.3333 by 533.3333
// that square is then divided into a 16 by 16 grid (each 33.3333 by 33.3333)
struct GridMapAreaChunk
{
    /* GridMapAreaHeader */
    uint32_t magic;             // magic value containing "AREA"
    uint16_t flags;             // 0x0001 --> All areas in the grid have the same data and is omitted in favor of "gridArea"
    uint16_t gridArea;          // if the 0x0001 flag is set then all area data is inside of this 16-bit integer (as opposed to the array data below)

    /* GridMapAreaData */
    // this following part is optional and only exists if 0x0001 flag is NOT set
    uint16_t data[16*16];
    // this data seems to be for deciding what zone gets discovered when you enter this area
    // not 100% sure how the data works but it's related to the zone exploration bit field
    // this data can be 0xffff, suggesting it's an int16 instead of uint16
    // see also:
    //  * player.cpp => Player::CheckAreaExploreAndOutdoor()
    //  * GridMap.cpp => TerrainInfo::GetAreaFlag()
    //  * AreaTable.dbc column 3
};

// Height data
// -----------
struct GridMapHeightHeader
{
    uint32_t magic;             // magic value containing "MHGT"
    uint32_t flags;             // 0x0001 --> no height data follows
                                // 0x0002 --> int16 height data follows
                                // 0x0004 --> int8 height data follows
                                // 0x0004 and 0x0002 are mutually exclusive
    float gridHeight;           // min height in this grid
    float gridMaxHeight;        // max height in this grid

    // if 0x0001 is set no data follows and all heights are equal to gridHeight

    // if 0x0002 is set:
    // multiplier = (gridMaxHeight - gridHeight) / 65535;
    // height = gridHeight + data * multiplier;
    uint16_t v9_data[129*129];
    uint16_t v8_data[128*128];
    
    // if 0x0004 is set:
    // multiplier = (gridMaxHeight - gridHeight) / 255;
    // height = gridHeight + data * multiplier;
    uint8_t v9_data[129*129];
    uint8_t v8_data[128*128];

    // if neither are set:
    // height = data
    float v9_data[129*129];
    float v8_data[128*128];
};

// Liquid Data
// -----------
struct GridMapLiquidHeader
{
    uint32_t magic;             // magic value containing "MLIQ"
    uint16 flags;               // defines which data exists (see below)
    // Simplicist liquid data (the liquidType and liquidLevel define simple liquid data, and is skipped in some case)
    uint16 liquidType;          // 0 - no liquid, 1 - magma, 2 - ocean, 3 - slime, 4 - water, 5 - dark water, 6 - WMO water
    // These following 4 values make up a rectangle that defines the liquid
    uint8 offsetX;              //
    uint8 offsetY;              //
    uint8 width;                // 
    uint8 height;               // 
    float liquidLevel;          // The height of the liquid, if it varies inside the rectangle liquidMap is used instead
    // Liquid Flag data (read if f0x0001 is NOT set in flags)
    uint16_t liquidEntry[16*16];// Defines ids into the LiquidTypeStore.dbc, specifying the type of the liquid. It's used for
                                // liquid where the liquidType is not enough, such as liquid that puts an aura on you when you're in it.
    uint8_t liquidFlag[16*16];  // This is the same as liquidType above, but each value is left-shifted, so it makes up a bit mask, for example
                                // (liquidFlag[i] & (1 << 4)) would yield true if the liquid is water

    // Liquid Map Data (read if f0x0002 is NOT set in flags)
    // If the entire liquid rectangle has the same height liquidLevel is used,
    // otherwise liquidMap contains the height for each position inside the
    // rectangle.
    float liquidMap[width*height];
};

/*
Note: To determine if you're in liquid or not, the core does not actually use
the map liquid data, but instead it uses VMAP, how that works you can read
about in vmap.cpp. However, once VMAP has determined you're in liquid, this
data is consulted to see what the liquid type is and its effects on you.
*/

// Holes Data
// ----------
// Not used by the core at the moment, and does not have a magic header value.
struct GridMapHoleData
{
    uint16 data[16*16];         // This data seems to be where the terrain
                                // sticks out and breaches through the height
                                // map, creating a "hole" that's higher up or
                                // lower down than the max or min height.
};

// 2) How map files are extracted
/*
The map extractions is a more complex operation than the dbc extraction, so I
will divide it into steps.

Step 1: As with the DBC we start by using libmpq to load all the .MPQ files we
are interested in from your WoW folder's Data/ directory.

Step 2: We then look at the Map.dbc file in the client's data. You can see the
layout of the Map.dbc in the core:
src/game/DBCStructure.h and then lookup the struct named MapEntry.
The only fields we care about for this procedure is the name and the id of the
map.

Step 3: We then go ahead and load the AreaTable.dbc. All we read from this dbc
is the id and the third column the area flags or exploredFlags, as the core
calls them. For the full structure you can go to:
src/game/DBCStructure.h and then lookup the struct named AreaTableEntry.

Step 4: Then we read the LiquidType.dbc. Again we read only the id and the 3rd
column, which in this case is the spell id. Full structure found at:
src/game/DBCStructure.h and then lookup the struct named LiquidTypeEntry.

Step 5: We then load what is called a WDT file, you can see its content in
wdt.h, but frankly it's not important since we only use it to determine which
grids have an associated ADT file. The file name is structured like this:
"World\Maps\%s\%s.wdt"
Where both %s is the name of the map, so loading azeroth would be:
"World\Maps\Azeroth\Azeroth.wdt"

Step 6: The maximum possible dimensions for a map are 64*64. In other words a
map can have a maximum of 4096 grids. The WDT file will specify which X,Y
combinations has an associated ADT file, if it does we go ahead and load that
ADT file using a path that looks like this:
"World\Maps\%s\%s_%u_%u.adt"
Where the format specifiers are, in order: map name, map name, x, y, so an ADT
file for Eastern Kingdoms (which is internally called Azeroth) might look like
this:
"World\Maps\Azeroth\Azeroth_32_24.adt"

Step 7: The ADT file is processed and its data interpreted, the resulting data
is then written to a file name like this:
"maps/%03u%02u%02u.map"
Where the format specifiers are, in order: map id, y, x (yes, not x, y), so
taking the same example file as before the output would be:
"maps/0002432.map"

The extraction of the ADT files data is a fairly complex procedure, so it will
be displayed in its own section below.
*/

// 3) WoW's usage of the cartesian coordinate system
/*
WoWs 3D space uses X and Y for the 2D plane along the ground, and Z for height.
Here are the cardinal directions and what axis is used:
north: +X
east:  -Y
west:  +Y
south: -X
up:    +Z
down:  -Z

If you're reading the extractor code, there is a further twist in that
the axes have been remapped: X axis to Z, Y to X and Z to Y.
*/

// 4) Structure of an ADT file
/*
The ADT file format has been documented here:
http://www.pxr.dk/wowdev/wiki/index.php?title=ADT/v18
This section will only make some notes required to understand the conversion
phase into map files described below.

The ADT file format is a 16*16 array of cells. Each cell being a 16th of the
total grid (533.333/16~=33.333).

MCNK chunk -- This chunk describes each cell in the grid. There exists one MCNK
              chunk for each cell (16*16=256 in total). The first chunk present
              is in the top-left corner of the grid, the second one is a step
              to the right, and so on.
              The MCNK chunk has a X,Y,Z coordinate to define where its
              top-left corner is in the world.

MCVT sub-chunk -- The MCVT sub-chunk is an optional sub-chunk to the MCNK
                  chunks. If it exists, it contains the 145 floating points
                  that make up the V9 and V8 data (read below).

V8 and V9 -- Each cell in the grid has an optional MCVT sub-chunk that contains
             9*9 vertices defining the height as that point in the map (a
             floating point number).
             There's also 8*8 vertices that defines the inner height of the 9*9
             vertices (see below).
             In total this means each cell has 9*9+8*8=145 total floating point
             numbers that make up the various heights across the cell.
             The V9 and V8 data make up in total 64 squares. These squares are
             made out of 5 floating points. Their X,Y in each cell is always
             the same, and they define the Z value at that oint.
             Note: All values in the V8 and V9 data are relative to the MCNK
             chunk of that cell. In other words MCNK->z_coord + v8/v9_float
             yields the actual position in the world.

MCLQ sub-chunk -- This data was deprecated in WotLK in favor of the new MH2O
                  chunk. Given that we're running TBC this chunk is what we use
                  for map-based liquid data. We also make use of the vmap data,
                  however. See vmap.cpp for more information.
*/


// 5) How ADT files are converted to map files
/*
=== Part One: Area Data ===
Step 1: Each cell of the grid is looped through, and the MCNK chunk of that
cell is retrieved and used to figure out the area id of that particular cell.
Remember the area flags we read from AreaTable.dbc? Using the area id of each
cell in the MCNK chunk we consult the DBC data to get the area flags of that
cell.

Step 2: If the area flags of all our cells are the same then
GridMapAreaHeader::gridArea will be present instead of the
GridMapAreaHeader::data in the final map file. Otherwise, each cell's area
flags will be written to the area data.

=== Part Two: Height Data ===
Step 1: Again, the MCNK chunk is consulted to read the initial state of the V9
and V8 data. We start by filling out the V9 and V8 data with the Z coordinate
of the MCNK chunk. This Z coordinate will be used if the cell does not contain
any v9 or v8 data, and the height is unanimous across the cell.

Step 2:
The V9 data is indexed like this in the MCVT sub-chunk: height[y * (16+1) + x],
where x and y are offsets in the range [0,8] from the top-left corner of the
cell.
The V8 data is indexed like this in the MCVT sub-chunk:
height[y * (16+1) + 8 + 1 + x], where x and y are offsets in the range [0,8)
from the top-left corner of the cell.
This means if we slap indices on the height map it will have the following
structure:

0       1       2       3       4       5       6       7       8
    9       10      11      12      13      14      15      16
17      18      19      20      21      22      23      24      25
    26      27      28      29      30      31      32      33
34      35      36      37      38      39      40      41      42
    43      44      45      46      47      48      49      50
51      52      53      54      55      56      57      58      59
    60      61      62      63      64      65      66      67
68      69      70      71      72      73      74      75      76
    77      78      79      80      81      82      83      84
85      86      87      88      89      90      91      92      93
    94      95      96      97      98      99      100     101
102     103     104     105     106     107     108     109     110
    111     112     113     114     115     116     117     118
119     120     121     122     123     124     125     126     127
    128     129     130     131     132     133     134     135
136     137     138     139     140     141     142     143     144

As you can see the V9 data are the rows with 9 indices, and the V8 data are the
in-between rows with 8 indices. While maybe not the prettiest ASCII drawing you
can hopefully see how the indices form a total of 64 squares over the cell.
The first square being made up of index 0, 1, 17, 18 and 9, the second of index
1, 2, 18 9 and 10, and so on.

Step 3: Mangos will then proceed to eliminate any height data that is part of
the deep ocean. If we have an actual height map (i.e. V9 and V8 data), mangos
will pack it into uint8_t or uint16_t if possible and the extractor is ran with
-f=1 (which is the default).

=== Part Three: Liquid Data ===
Step 1: We check through each MCNK chunk to see if it has any MCLQ sub-chunks.

Step 2: Liquid height data is stored in a 9x9 2-dimensional array:
MCLQ->liquid[y][x].height
Just as with the V9 data this makes up 64 squares over the cell that describes
the height of the liquid at those points.
We proceed to extract these height values; these values are not relative.

Step 3: Again, referring back to the V8 data, there exists an 8x8 array that
defines flags for each square in the 9x9 liquid height data.
MCLQ->flags[y][x]

Step 4: If the flags for a square in the 9x9 array is 0x0F that square is
marked as "not showing up", and is never saved to the resulting liquid height
data.

Step 5: If the flags contain 0x80 we know that that square is part of the deep
ocean, and we mark it accodringly in the resulting flags we will write to our
map files.

Step 6: Every other flag we write to our map files will be from the flags of
that cell, and not the individual liquid flags. The  various values and their
meanings are
MCNK->flags:
0x04: water
0x08: ocean
0x10: magma/slime

Step 7: The data we have gathered for the liquid is now converted into the
format described above in this file.

=== Part Four: Holes Data ===
TODO: This data needs more research.
*/
