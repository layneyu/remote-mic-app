#define _POSIX_C_SOURCE 200809L

#include "command_sequence.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static char *trim(char *value) {
    while (isspace((unsigned char)*value)) {
        value++;
    }
    char *end = value + strlen(value);
    while (end > value && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return value;
}

void command_sequence_state_init(CommandSequenceState *state, const char *path) {
    memset(state, 0, sizeof(*state));
    state->path = path;
}

static bool file_changed(const struct stat *metadata, const CommandSequenceState *state) {
    return !state->initialized ||
        state->file_size != (uintmax_t)metadata->st_size ||
        state->file_mtime_sec != (int64_t)metadata->st_mtim.tv_sec ||
        state->file_mtime_nsec != (int64_t)metadata->st_mtim.tv_nsec;
}

bool command_sequence_next(
    const char *path,
    CommandSequenceState *state,
    char command[COMMAND_SEQUENCE_COMMAND_MAX],
    size_t *index,
    size_t *count
) {
    struct stat metadata;
    if (path == NULL || stat(path, &metadata) != 0) {
        return false;
    }
    if (file_changed(&metadata, state)) {
        state->next_index = 0;
        state->file_size = (uintmax_t)metadata.st_size;
        state->file_mtime_sec = (int64_t)metadata.st_mtim.tv_sec;
        state->file_mtime_nsec = (int64_t)metadata.st_mtim.tv_nsec;
        state->initialized = true;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return false;
    }
    size_t selected_index = state->next_index;
    size_t command_count = 0;
    bool selected = false;
    char line[COMMAND_SEQUENCE_COMMAND_MAX];
    while (fgets(line, sizeof(line), file) != NULL) {
        char *value = trim(line);
        if (*value == '\0' || *value == '#') {
            continue;
        }
        if (command_count == selected_index) {
            snprintf(command, COMMAND_SEQUENCE_COMMAND_MAX, "%s", value);
            selected = true;
        }
        command_count++;
    }
    fclose(file);
    if (command_count == 0) {
        return false;
    }
    if (!selected) {
        selected_index = 0;
        FILE *second_pass = fopen(path, "r");
        if (second_pass == NULL) {
            return false;
        }
        while (fgets(line, sizeof(line), second_pass) != NULL) {
            char *value = trim(line);
            if (*value == '\0' || *value == '#') {
                continue;
            }
            if (selected_index == 0) {
                snprintf(command, COMMAND_SEQUENCE_COMMAND_MAX, "%s", value);
                break;
            }
        }
        fclose(second_pass);
    }
    *index = selected_index;
    *count = command_count;
    state->next_index = (selected_index + 1) % command_count;
    return true;
}
