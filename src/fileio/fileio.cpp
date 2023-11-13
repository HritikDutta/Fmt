#include "fileio.h"

#include "core/logger.h"
#include "containers/string.h"
#include "containers/bytes.h"
#include "containers/darray.h"

String file_load_string(const String& filepath)
{
    char path_c_str[256] = {};  // Since paths need to be null terminated
    String path = ref(path_c_str);

    if (filepath[filepath.size-1] == '\0')
        path = filepath;
    else
        string_copy_into(path, filepath);

    // TODO: Strings are not always null terminated. Do something about that!
    FILE* file = fopen(path.data, "rb");
    if (!file)
    {
        print_error("Error opening file! (errno: \"%\", filepath: \"%\")\n", strerror(errno), filepath);
        return String {};
    }

    fseek(file, 0, SEEK_END);
    int length = ftell(file);
    fseek(file, 0, SEEK_SET);

    DynamicArray<char> output = {};
    resize(output, length + 1);
    fread(output.data, sizeof(char), length, file);
    output.data[length] = '\0';

    fclose(file);

    return String { output.data, output.capacity - 1 };
}

Bytes file_load_bytes(const String& filepath)
{
    char path_c_str[256] = {};  // Since paths need to be null terminated
    String path = ref(path_c_str);

    if (filepath[filepath.size-1] == '\0')
        path = filepath;
    else
        string_copy_into(path, filepath);

    // TODO: Strings are not always null terminated. Do something about that!
    FILE* file = fopen(path.data, "rb");
    if (!file)
    {
        print_error("Error opening file! (errno: \"%\", filepath: \"%\")\n", strerror(errno), filepath);
        return Bytes {};
    }

    fseek(file, 0, SEEK_END);
    int length = ftell(file);
    fseek(file, 0, SEEK_SET);

    DynamicArray<u8> output = {};
    resize(output, length);
    fread(output.data, sizeof(u8), length, file);

    fclose(file);

    return Bytes { output.data, output.capacity };
}

void file_write_string(const String& filepath, const String& string)
{
    char path_c_str[256] = {};  // Since paths need to be null terminated
    String path = ref(path_c_str);

    if (filepath[filepath.size-1] == '\0')
        path = filepath;
    else
        string_copy_into(path, filepath);

    // TODO: Strings are not always null terminated. Do something about that!
    FILE* file = fopen(path.data, "wb");
    if (!file)
    {
        print_error("Error opening file! (errno: \"%\", filepath: \"%\")\n", strerror(errno), filepath);
        return;
    }

    u64 written = fwrite(string.data, sizeof(u8), string.size, file);
    if (written != string.size)
    {
        print_error("Error writing to file! (errno: \"%\", filepath: \"%\")\n", strerror(errno), filepath);
        fclose(file);
        return;
    }

    int error = fclose(file);
    if (error)
    {
        print_error("Error closing file! (errno: \"%\", filepath: \"%\")\n", strerror(errno), filepath);
        return;
    }
}

void file_write_bytes(const String& filepath, const Bytes& bytes)
{
    char path_c_str[256] = {};  // Since paths need to be null terminated
    String path = ref(path_c_str);

    if (filepath[filepath.size-1] == '\0')
        path = filepath;
    else
        string_copy_into(path, filepath);

    // TODO: Strings are not always null terminated. Do something about that!
    FILE* file = fopen(path.data, "wb");
    if (!file)
    {
        print_error("Error opening file! (errno: \"%\", filepath: \"%\")\n", strerror(errno), filepath);
        return;
    }

    u64 written = fwrite(bytes.data, sizeof(u8), bytes.size, file);
    if (written != bytes.size)
    {
        print_error("Error writing to file! (errno: \"%\", filepath: \"%\")\n", strerror(errno), filepath);
        fclose(file);
        return;
    }

    int error = fclose(file);
    if (error)
    {
        print_error("Error closing file! (errno: \"%\", filepath: \"%\")\n", strerror(errno), filepath);
        return;
    }
}