#ifndef LOAD_LIB_H
#define LOAD_LIB_H

#include <cstdint>

typedef int64_t int64;
typedef int32_t int32;
typedef int16_t int16;
typedef int8_t int8;
typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

#define FILE_FORMAT_VERSION 18

//
// File version chunk
//
struct file_MVER
{
    union
    {
        uint32 fcc;
        char fcc_txt[4];
    };
    uint32 size;
    uint32 ver;
};

class FileLoader
{
    uint8* data;
    uint32 data_size;

public:
    virtual bool prepareLoadedData();
    uint8* GetData() { return data; }
    uint32 GetDataSize() { return data_size; }

    file_MVER* version;
    FileLoader();
    ~FileLoader();
    bool loadFile(char* filename, bool log = true);
    virtual void free();
};
#endif
