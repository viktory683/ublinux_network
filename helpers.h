#include <stdbool.h>
#include <stdint.h>
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

int is_ip_valid(const char* ip_address) {
    // 1 | invalid symbols
    char* validSymbols = "1234567890.";
    for (int i = 0; i < strlen(ip_address); i++) {
        if (!strchr(validSymbols, ip_address[i])) {
            return 1;
        }
    }

    // 2 | octets != 4
    int point_count = 0;
    for (int i = 0; i < strlen(ip_address); i++) {
        if (ip_address[i] == '.') {
            point_count++;
        }
    }
    if (point_count != 3) {
        return 2;
    }

    // 3 | 0 <= octet < 256
    int octets[4];
    sscanf(ip_address, "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]);
    for (int i = 0; i < 4; i++) {
        if (octets[i] < 0 || octets[i] > 255) {
            return 3;
        }
    }

    return 0;
}

void printBinary(uint32_t num) {
    // Start from the most significant bit and print each bit
    for (int i = 31; i >= 0; i--) {
        // Shift the number i bits to the right and check the LSB
        int bit = (num >> i) & 1;
        printf("%d", bit);
    }
    printf("\n");
}

int is_mask_valid(const char* raw_mask) {
    int ip_valid = is_ip_valid(raw_mask);
    if (ip_valid) {
        return ip_valid;
    }

    int mask_octets[4];
    sscanf(raw_mask, "%d.%d.%d.%d", &mask_octets[0], &mask_octets[1], &mask_octets[2], &mask_octets[3]);

    uint32_t mask = 0;
    for (int i = 0; i < 4; i++) {
        mask <<= 8;
        mask += mask_octets[i];
    }

    bool end = false;
    for (int i = 31; i >= 0; i--) {
        if ((mask >> i) & 1) {
            if (end) {
                return 4;
            }
        } else {
            end = true;
        }
    }

    return 0;
}
