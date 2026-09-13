#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include "command_sequence.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_commands(const char *path, const char *content) {
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fputs(content, file);
    fclose(file);
}

int main(void) {
    char path[] = "/tmp/xiaomi-command-sequence-XXXXXX";
    int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    close(descriptor);
    write_commands(path, "# comments are ignored\none\n\ntwo\n");

    CommandSequenceState state;
    command_sequence_state_init(&state, path);
    char command[COMMAND_SEQUENCE_COMMAND_MAX];
    size_t index;
    size_t count;
    assert(command_sequence_next(path, &state, command, &index, &count));
    assert(index == 0 && count == 2 && strcmp(command, "one") == 0);
    assert(command_sequence_next(path, &state, command, &index, &count));
    assert(index == 1 && count == 2 && strcmp(command, "two") == 0);
    assert(command_sequence_next(path, &state, command, &index, &count));
    assert(index == 0 && strcmp(command, "one") == 0);

    write_commands(path, "three\nfour\nfive\n");
    assert(command_sequence_next(path, &state, command, &index, &count));
    assert(index == 0 && count == 3 && strcmp(command, "three") == 0);
    unlink(path);
    return 0;
}
