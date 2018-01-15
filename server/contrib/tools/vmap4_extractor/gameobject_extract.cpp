#include "adtfile.h"
#include "dbcfile.h"
#include "model.h"
#include "vmapexport.h"
#include <algorithm>
#include <stdio.h>

bool ExtractSingleModel(std::string& origPath, std::string& fixedName,
    std::set<std::string>& /*failedPaths*/)
{
    char const* ext = GetExtension(GetPlainName(origPath.c_str()));

    // < 3.1.0 ADT MMDX section store filename.mdx filenames for corresponded
    // .m2 file
    if (!strcmp(ext, ".mdx"))
    {
        // replace .mdx -> .m2
        origPath.erase(origPath.length() - 2, 2);
        origPath.append("2");
    }
    // >= 3.1.0 ADT MMDX section store filename.m2 filenames for corresponded
    // .m2 file
    // nothing do

    fixedName = GetPlainName(origPath.c_str());

    std::string output(
        szWorkDirWmo); // Stores output filename (possible changed)
    output += "/";
    output += fixedName;

    if (FileExists(output.c_str()))
        return true;

    Model mdl(origPath); // Possible changed fname
    if (!mdl.open())
        return false;

    return mdl.ConvertToVMAPModel(output.c_str());
}

void ExtractGameobjectModels()
{
    printf("Extracting GameObject models...");
    DBCFile dbc("DBFilesClient\\GameObjectDisplayInfo.dbc");
    if (!dbc.open())
    {
        printf("Fatal error: Invalid GameObjectDisplayInfo.dbc file format!\n");
        exit(1);
    }

    std::string basepath = szWorkDirWmo;
    basepath += "/";
    std::string path;
    std::set<std::string> failedPaths;

    FILE* model_list =
        fopen((basepath + "temp_gameobject_models").c_str(), "wb");

    for (const auto& elem : dbc)
    {
        path = elem.getString(1);

        if (path.length() < 4)
            continue;

        fixnamen((char*)path.c_str(), path.size());
        char* name = GetPlainName((char*)path.c_str());
        fixname2(name, strlen(name));

        char const* ch_ext = GetExtension(name);
        if (!ch_ext)
            continue;

        // strToLower(ch_ext);

        bool result = false;
        if (!strcmp(ch_ext, ".wmo"))
        {
            result = ExtractSingleWmo(path);
        }
        else if (!strcmp(ch_ext, ".mdl"))
        {
            // TODO: extract .mdl files, if needed
            continue;
        }
        else // if (!strcmp(ch_ext, ".mdx") || !strcmp(ch_ext, ".m2"))
        {
            std::string fixedName;
            result = ExtractSingleModel(path, fixedName, failedPaths);
        }

        if (result)
        {
            uint32 displayId = elem.getUInt(0);
            uint32 path_length = strlen(name);
            fwrite(&displayId, sizeof(uint32), 1, model_list);
            fwrite(&path_length, sizeof(uint32), 1, model_list);
            fwrite(name, sizeof(char), path_length, model_list);
        }
    }

    fclose(model_list);

    if (!failedPaths.empty())
    {
        printf("Warning: Some models could not be extracted, see below\n");
        for (const auto& failedPath : failedPaths)
            printf("Could not find file of model %s\n", failedPath.c_str());
        printf(
            "A few of these warnings are expected to happen, so don't be "
            "alarmed!\n");
    }

    printf("Done!\n");
}
