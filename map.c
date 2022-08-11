#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "map.h"

Map g_map = {0};

char *slurp_file(const char *file_path, size_t *size)
{
    char *buffer = NULL;

    FILE *f = fopen(file_path, "rb");
    if (f == NULL) {
        goto error;
    }

    if (fseek(f, 0, SEEK_END) < 0) {
        goto error;
    }

    long m = ftell(f);
    if (m < 0) {
        goto error;
    }

    buffer = malloc(sizeof(char) * m);
    if (buffer == NULL) {
        goto error;
    }

    if (fseek(f, 0, SEEK_SET) < 0) {
        goto error;
    }

    size_t n = fread(buffer, 1, m, f);
    if (n != (size_t) m) {
        goto error;
    }

    if (ferror(f)) {
        goto error;
    }

    if (size)
        *size = n;

    fclose(f);

    return buffer;

error:
    if (f) {
        fclose(f);
    }

    if (buffer) {
        free(buffer);
    }

    return NULL;
}

bool load_map(char *path) {
    char *map_str;
    size_t size;

    map_str = slurp_file(path, &size);
    if (map_str == NULL) {
        fprintf(stderr, "Could not load file %s\n", path);
        return false;
    }
    //calculate line number
    size_t line_length;
    for (line_length = 0; line_length < size && map_str[line_length] != '\n'; line_length++);
    size_t num_lines = (size + 1) / (line_length + 1);
    if (line_length < 1) {printf("%s: empty map!\n", path); 
    exit(0);
    }
    g_map.matrix = malloc(num_lines * sizeof(int8_t *));
    if (g_map.matrix == NULL) {
        fprintf(stderr, "Could not allocate memory for the map!\n");
        return false;
    }

    //from this point on, line length is measured in tokens (numbers)
    size_t cur_line = 0;
    size_t line_len_in_tokens = 0;
    for (size_t i = 0; i < size; i += line_length + 1) {
        size_t num_count = 0;
        size_t j;
        bool in = false;
        //check number count
        for (j = 0; map_str[i + j] != '\n' && i + j < size; j++) {
            if (in && !isdigit(map_str[i + j])) {
                in = false;
            }
            else if (!in && isdigit(map_str[i + j])) {
                in = true;
                num_count++;
            }
        }
        if (i + j < size) map_str[i + j] = '\0';
        if (num_count < 1) {
            printf("Wrong map format. A map should have at least one column\n");
            return false;
        }
        if (line_len_in_tokens == 0) line_len_in_tokens = num_count;
        if (num_count != line_len_in_tokens) {
            fprintf(stderr, "Wrong map format at line %zu:%zu numbers (lines don't have the same length)!\n", cur_line, num_count);
            return false;
        }
        if (cur_line == num_lines) {
            fprintf(stderr, "Too many lines when parsing map!\n");
            return false;
        }
        g_map.matrix[cur_line] = malloc(line_len_in_tokens * sizeof(int8_t));
        if (g_map.matrix[cur_line] == NULL) {
            fprintf(stderr, "Could not allocate memory for the map!\n");
            return false;
        }
        char *num;
        j = 0;
        num = strtok(&map_str[i], " ");
        g_map.matrix[cur_line][j++] = atoi(num);
        while ((num = strtok(NULL, " ")) != NULL) {
            g_map.matrix[cur_line][j++] = atoi(num);
        }
        cur_line++;
    }
    assert(cur_line == num_lines);
    g_map.width = line_len_in_tokens;
    g_map.height = num_lines;

    free(map_str);
    return true;
}

Point find_random_free_spot_on_a_map(void) {
    short x, y;
    while (g_map.matrix[y = rand() % g_map.height][x = rand() % g_map.width] != MAP_FREE);
    Point point = {x, y};
    return point;
}

//not the most efficient way, but allows to check if there are free points at all
//TODO: index all free spaces and make this function extra fast and safe
Point find_random_free_spot_on_a_map_safe(void) {
    Point free_points[g_map.height * g_map.width];
    int sp = 0;
    Point point;
    for (int i = 0; i < g_map.height; i++) {
        for (int j = 0; j < g_map.width; j++) {
            if (g_map.matrix[i][j] == MAP_FREE) {
                point.y = i;
                point.x = j;
                free_points[sp++] = point;
            }
        }
    }
    assert(sp > 0);
    return free_points[rand() % sp];
}