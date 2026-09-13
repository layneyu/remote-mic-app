#ifndef XIAOMI_REMOTE_COMMAND_SEQUENCE_H
#define XIAOMI_REMOTE_COMMAND_SEQUENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    COMMAND_SEQUENCE_COMMAND_MAX = 4096,
};

typedef struct {
    const char *path;
    bool initialized;
    uintmax_t file_size;
    int64_t file_mtime_sec;
    int64_t file_mtime_nsec;
    size_t next_index;
} CommandSequenceState;

void command_sequence_state_init(CommandSequenceState *state, const char *path);
bool command_sequence_next(
    const char *path,
    CommandSequenceState *state,
    char command[COMMAND_SEQUENCE_COMMAND_MAX],
    size_t *index,
    size_t *count
);

#endif
