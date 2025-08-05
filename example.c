#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#include "json_parser.h"

char *read_entire_file(const char *filename, ptrdiff_t *out_len) {
    // Open the file for reading
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("File opening failed");
        return NULL;
    }

    // Move the file pointer to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if (file_size == -1) {
        perror("Failed to determine file size");
        fclose(file);
        return NULL;
    }

    // Move the file pointer back to the beginning of the file
    fseek(file, 0, SEEK_SET);

    // Allocate memory to store the file content
    char *buffer = (char *)malloc(file_size + 1);  // +1 for the null terminator
    if (!buffer) {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    // Read the file content into the buffer
    long bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        perror("Failed to read entire file");
        free(buffer);
        fclose(file);
        return NULL;
    }

    // Null-terminate the string
    buffer[file_size] = '\0';

    // Set the output length
    if (out_len) {
        *out_len = file_size;
    }

    // Close the file and return the buffer
    fclose(file);
    return buffer;
}


int main()
{
  char *file_name = "small.json";
  ptrdiff_t len;
  char *file_contents = read_entire_file(file_name, &len);

  Json_Parser p[1];
  json_open_buffer(p, file_contents, len);
  const Json_View *json = json_parse(p);
  assert(json_type(json) == JSON_ARRAY);
  
  return 0;
}
