/*
DBC -- DBFilesClient
index:
1) Structure of a dbc file
2) How dbc files are extracted
*/

// 1) Structure of a dbc file

// each file starts with a header:
struct alignas(1) dbc_header
{
    char     magic[4];              // contains {'W', 'D', 'B', 'C'}
    uint32_t row_count;             // number of rows
    uint32_t column_count;          // number of column in each row
    uint32_t row_size;              // size of each row in bytes, this will always be 4*column_count
    uint32_t string_size;           // total size of the string data (at the end of the file)
};

// full file looks like:
struct alignas(1) dbc_file
{
    struct dbc_header header;
    uint8_t records[row_count][row_size];
    uint8_t string_data[string_size];
};

/*
each column is 4 bytes and can contain one of the following types:
unsigned 32 bit integer
signed 32 bit integer
32 bit float
text string

what types are contained in a dbc file is not encoded in the file, you
need to know what specific data is in the file before you can interpret
what each column means

integers are little endian, floats are what IA32 uses (IEEE754)

text strings are a 32 bit offset into the string data, it looks like
the first string in the string data is always an empty string, though
that might not always be the case. The string encoding uses 1 byte for
most (all?) characters so it's probably either ASCII or utf8

the string data is just a big blob of strings all split up by \0 bytes
strings come in up to 16 variants, each representing one of the possible
locales, however, your client only has one locale filled in, in other words
the other 15 strings are just null-bytes. This is at least true for the english
client, it's possible the international clients have their own language +
english, but this hasn't been verified by us.
*/

// 2) How dbc files are extracted
/*
We use a library called libmpq that is used to read the .MPQ files located in
the Data directory of your World of Warcraft installation (such as common.MPQ
or expansion.MPQ).

An MPQ file is a Blizzard proprietary archive format. Inside these MPQ files,
we can find files with the file ending ".dbc". We simply extract them as is.
In other words, the .dbc files the core is using are found exactly as is in the
client.
*/
