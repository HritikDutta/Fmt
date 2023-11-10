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
    gn_assert_with_message(file, "Error opening file! (errno: \"%\", filepath: \"%\")", strerror(errno), filepath);

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
    gn_assert_with_message(file, "Error opening file! (errno: \"%\", filepath: \"%\")", strerror(errno), filepath);

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
    gn_assert_with_message(file, "Error opening file! (errno: \"%\", filepath: \"%\")", strerror(errno), filepath);

    u64 written = fwrite(string.data, sizeof(u8), string.size, file);
    gn_assert_with_message(written == string.size, "Error writing to file! (errno: \"%\", filepath: \"%\")", strerror(errno), filepath);

    int success = fclose(file);
    gn_assert_with_message(success == 0, "Error closing file! (errno: \"%\", filepath: \"%\")", strerror(errno), filepath);
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
    gn_assert_with_message(file, "Error opening file! (errno: \"%\", filepath: \"%\")", strerror(errno), filepath);

    u64 written = fwrite(bytes.data, sizeof(u8), bytes.size, file);
    gn_assert_with_message(written == bytes.size, "Error writing to file! (errno: \"%\", filepath: \"%\")", strerror(errno), filepath);

    int success = fclose(file);
    gn_assert_with_message(success == 0, "Error closing file! (errno: \"%\", filepath: \"%\")", strerror(errno), filepath);
}