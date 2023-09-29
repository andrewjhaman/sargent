#pragma once


#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))


struct ReadFileResult
{
    int64_t size;
    void* data;
};


#include <cstdio>
ReadFileResult read_entire_file(char* path) {
    ReadFileResult result = {};


    FILE* file = fopen(path, "r");
    if (!file) printf("utils.h Error: Could not open file %s\n", path);

    int success = fseek(file, 0, SEEK_END);
    if (success != 0) printf("utils.h Error: Could not seek to end of file %s\n", path);
    result.size = ftell(file);

    rewind(file);

    result.data = malloc(result.size);
    if(!result.data)
        printf("utils.h Error: Could not allocate memory of size %llu for file %s\n", result.size, path);
    size_t read_item_count = fread(result.data, result.size, 1, file);
    if(read_item_count != 1)
    {
        printf("utils.h Error: Could not read from file %s\n", path);
        free(result.data);
    }

    fclose(file);

    return result;
}
