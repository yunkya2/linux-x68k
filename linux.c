/* Linux kernel loader for X68000 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <x68k/dos.h>

#define LOAD_ALIGN     8192U
#define X_FILE_TYPE    0x03000000UL
#define MAX_BLOCK_SIZE 0x01000000
#define COMMAND_LINE_SIZE 256
#define DEFAULT_COMMAND_LINE "init=/bin/sh"

struct lasciiz {
    /* Human68k command line: length byte followed by an ASCIIZ string. */
    uint8_t length;
    char string[COMMAND_LINE_SIZE];
};

static int kernel_path(char *path, size_t size, const char *loader_path)
{
    const char *separator = NULL;
    const char *p;
    size_t directory_length;

    for (p = loader_path; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/' || *p == ':')
            separator = p;
    }

    directory_length = separator ? (size_t)(separator + 1 - loader_path) : 0;
    if (directory_length + sizeof("linux.sys") > size)
        return -1;

    memcpy(path, loader_path, directory_length);
    memcpy(path + directory_length, "linux.sys", sizeof("linux.sys"));
    return 0;
}

static int build_command_line(struct lasciiz *command_line,
                              int argc, char **argv)
{
    size_t length = 0;
    int i;

    if (argc == 1) {
        memcpy(command_line->string, DEFAULT_COMMAND_LINE,
               sizeof(DEFAULT_COMMAND_LINE));
        command_line->length = sizeof(DEFAULT_COMMAND_LINE) - 1;
        return 0;
    }

    for (i = 1; i < argc; i++) {
        size_t argument_length = strlen(argv[i]);

        if (length + (i > 1) + argument_length >=
            sizeof(command_line->string))
            return -1;
        if (i > 1)
            command_line->string[length++] = ' ';
        memcpy(command_line->string + length, argv[i], argument_length);
        length += argument_length;
    }
    command_line->string[length] = '\0';
    command_line->length = length;
    return 0;
}

static void __attribute__((noreturn)) start_kernel(void *entry,
                                                   const struct lasciiz *command_line)
{
    register const struct lasciiz *a2 __asm__("a2") = command_line;
    register void *a1 __asm__("a1") = entry;

    __asm__ volatile ("jmp %1@" : : "a" (a2), "a" (a1));
    __builtin_unreachable();
}

int main(int argc, char **argv)
{
    struct dos_psp *psp = _dos_getpdb();
    static char path[260];
    static struct lasciiz command_line;
    uintptr_t load_address;
    uintptr_t limit_address;
    void *program_end;
    int maximum;
    int result;

    if (argc < 1 || argv[0] == NULL) {
        fprintf(stderr, "Failed to determine loader path\n");
        return 1;
    }
    if (kernel_path(path, sizeof(path), argv[0]) < 0) {
        fprintf(stderr, "Kernel path is too long\n");
        return 1;
    }
    if (build_command_line(&command_line, argc, argv) < 0) {
        fprintf(stderr, "Kernel command line is too long\n");
        return 1;
    }

    program_end = sbrk(0);
    if (program_end == (void *)-1) {
        fprintf(stderr, "Failed to determine loader memory size\n");
        return 1;
    }

    /* An oversized request returns the largest size in the low 24 bits. */
    maximum = _dos_setblock(psp, MAX_BLOCK_SIZE);
    if ((uint32_t)maximum >> 24 != 0x81) {
        fprintf(stderr, "Failed to determine maximum memory block size\n");
        return 1;
    }
    maximum &= 0x00ffffff;
    if (_dos_setblock(psp, maximum) < 0) {
        fprintf(stderr, "Failed to expand memory block\n");
        return 1;
    }

    /* Keep the complete loader allocation below the loaded kernel. */
    load_address = ((uintptr_t)program_end + LOAD_ALIGN - 1) &
                   ~(uintptr_t)(LOAD_ALIGN - 1);
    limit_address = (uintptr_t)psp + (uint32_t)maximum;
    if (load_address >= limit_address) {
        fprintf(stderr, "No memory available for linux.sys\n");
        return 1;
    }

    /* linux.sys contains an X executable despite its .sys extension. */
    result = _dos_loadonly((const char *)((uintptr_t)path | X_FILE_TYPE),
                           (void *)load_address, (void *)limit_address);
    if (result < 0) {
        fprintf(stderr, "Failed to load %s: %d\n", path, result);
        return 1;
    }

    start_kernel((void *)load_address, &command_line);
}
