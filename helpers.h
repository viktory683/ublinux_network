#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Executes a system command and stores the command output in a dynamically allocated memory.
 *
 * Written with phind (https://www.phind.com/agent?cache=clk6itjk4005jl708blm5tvza)
 *
 * @param command The command to execute.
 * @param output A pointer to a character pointer where the command output will be stored.
 *               The memory for the output will be dynamically allocated and should be freed by the caller.
 */
void execute_command(const char* command, char** output) {
    FILE* fp;
    char buffer[1024];
    size_t outputSize = 0;
    size_t bufferSize = sizeof(buffer);

    fp = popen(command, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to execute command\n");
        return;
    }

    *output = NULL;

    while (fgets(buffer, bufferSize, fp) != NULL) {
        size_t lineSize = strlen(buffer);

        char* newOutput = realloc(*output, outputSize + lineSize + 1);
        if (newOutput == NULL) {
            fprintf(stderr, "Failed to allocate memory\n");
            free(*output);
            *output = NULL;
            break;
        }

        *output = newOutput;
        memcpy(*output + outputSize, buffer, lineSize);
        outputSize += lineSize;
    }

    if (*output != NULL) {
        (*output)[outputSize] = '\0';
    }

    pclose(fp);
}

int safe_check() {
    char* commands[] = { "nmcli", "jc" };
    int commands_count = sizeof(commands) / sizeof(commands[0]);

    bool error = false;
    for (int i = 0; i < commands_count; i++) {
        char command[sizeof("which  > /dev/null 2>&1") + sizeof(commands[i])] = "which ";
        strcat(command, commands[i]);
        strcat(command, " > /dev/null 2>&1");

        if (system(command)) {
            error = true;
            fprintf(stderr, "command: '%s' is not found in system\n", commands[i]);
        }
    }
    return error;
}