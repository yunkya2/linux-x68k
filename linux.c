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
#define DEFAULT_KERNEL_NAME "linux.sys"
#define DEFAULT_ROOTFS_NAME "linuxroot.img"
#define ROOTFS_PARAMETER "rootfs_image="
#define RAMEND_PARAMETER " ramend=0x%08lx"

struct lasciiz {
    /* Human68k command line: length byte followed by an ASCIIZ string. */
    uint8_t length;
    char string[COMMAND_LINE_SIZE];
};

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [-k kernel] [-r rootfs] [-f free-memory] "
            "[--] [kernel-arguments...]\n",
            program);
}

static int parse_memory_size(const char *string, uint32_t *size)
{
    uint32_t value = 0;
    uint32_t multiplier = 1;
    const char *p = string;

    if (*p < '0' || *p > '9')
        return -1;
    do {
        uint32_t digit = *p++ - '0';

        if (value > (UINT32_MAX - digit) / 10)
            return -1;
        value = value * 10 + digit;
    } while (*p >= '0' && *p <= '9');

    if (*p == 'K' || *p == 'k') {
        multiplier = 1024;
        p++;
    } else if (*p == 'M' || *p == 'm') {
        multiplier = 1024 * 1024;
        p++;
    }
    if (*p != '\0' || value > UINT32_MAX / multiplier)
        return -1;

    *size = value * multiplier;
    return 0;
}

static int default_path(char *path, size_t size, const char *loader_path,
                        const char *file_name)
{
    const char *separator = NULL;
    const char *p;
    size_t directory_length;
    size_t file_name_length = strlen(file_name) + 1;

    for (p = loader_path; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/' || *p == ':')
            separator = p;
    }

    directory_length = separator ? (size_t)(separator + 1 - loader_path) : 0;
    if (directory_length + file_name_length > size)
        return -1;

    memcpy(path, loader_path, directory_length);
    memcpy(path + directory_length, file_name, file_name_length);
    return 0;
}

static int copy_path(char *path, size_t size, const char *source)
{
    size_t length = strlen(source) + 1;

    if (length > size)
        return -1;
    memcpy(path, source, length);
    return 0;
}

static int full_path(char *path, size_t size)
{
    char source[260];
    char directory[256];
    const char *relative;
    char drive;
    int drive_number;
    int length;

    if (copy_path(source, sizeof(source), path) < 0)
        return -1;

    if (((source[0] >= 'A' && source[0] <= 'Z') ||
         (source[0] >= 'a' && source[0] <= 'z')) && source[1] == ':') {
        drive = source[0];
        relative = source + 2;
    } else {
        drive = 'A' + _dos_curdrv();
        relative = source;
    }
    if (drive >= 'a' && drive <= 'z')
        drive -= 'a' - 'A';

    if (*relative == '/' || *relative == '\\') {
        length = snprintf(path, size, "%c:%s", drive, relative);
    } else {
        drive_number = drive - 'A' + 1;
        if (_dos_curdir(drive_number, directory) < 0)
            return -1;
        if (directory[0] != '\0')
            length = snprintf(path, size, "%c:\\%s\\%s", drive,
                              directory, relative);
        else
            length = snprintf(path, size, "%c:\\%s", drive, relative);
    }

    if (length < 0 || (size_t)length >= size)
        return -1;
    return 0;
}

static int linux_path(char *path, size_t size)
{
    size_t length = strlen(path);
    size_t i;

    if (((path[0] >= 'A' && path[0] <= 'Z') ||
         (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':') {
        char drive = path[0];
        int add_separator = path[2] != '\0' &&
                            path[2] != '/' && path[2] != '\\';

        if (length + 5 + add_separator > size)
            return -1;
        if (drive >= 'A' && drive <= 'Z')
            drive += 'a' - 'A';
        memmove(path + 6 + add_separator, path + 2, length - 1);
        memcpy(path, "/mnt/", 5);
        path[5] = drive;
        if (add_separator)
            path[6] = '/';
        length += 4 + add_separator;
    }

    for (i = 0; i < length; i++) {
        if (path[i] == '\\')
            path[i] = '/';
    }
    return 0;
}

static int build_command_line(struct lasciiz *command_line,
                              const char *rootfs_path,
                              uintptr_t ramend,
                              int argc, char **argv, int first_argument)
{
    size_t rootfs_path_length = strlen(rootfs_path);
    size_t length = sizeof(ROOTFS_PARAMETER) - 1 + rootfs_path_length;
    int ramend_length;
    int i;

    if (length >= sizeof(command_line->string))
        return -1;
    memcpy(command_line->string, ROOTFS_PARAMETER,
           sizeof(ROOTFS_PARAMETER) - 1);
    memcpy(command_line->string + sizeof(ROOTFS_PARAMETER) - 1,
           rootfs_path, rootfs_path_length);

    ramend_length = snprintf(command_line->string + length,
                             sizeof(command_line->string) - length,
                             RAMEND_PARAMETER, (unsigned long)ramend);
    if (ramend_length < 0 ||
        (size_t)ramend_length >= sizeof(command_line->string) - length)
        return -1;
    length += ramend_length;

    for (i = first_argument; i < argc; i++) {
        size_t argument_length = strlen(argv[i]);

        if (length + 1 + argument_length >= sizeof(command_line->string))
            return -1;
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
    static char kernel_path[260];
    static char rootfs_path[260];
    static struct lasciiz command_line;
    uintptr_t load_address;
    uintptr_t limit_address;
    void *program_end;
    int first_kernel_argument;
    uint32_t free_memory = 256 * 1024;
    uint32_t allocation_size;
    int maximum;
    int result;
    int rootfs_option = 0;
    int i;

    if (argc < 1 || argv[0] == NULL) {
        fprintf(stderr, "Failed to determine loader path\n");
        return 1;
    }

    if (default_path(kernel_path, sizeof(kernel_path), argv[0],
                     DEFAULT_KERNEL_NAME) < 0 ||
        default_path(rootfs_path, sizeof(rootfs_path), argv[0],
                     DEFAULT_ROOTFS_NAME) < 0) {
        fprintf(stderr, "Loader path is too long\n");
        return 1;
    }

    first_kernel_argument = argc;
    for (i = 1; i < argc;) {
        if (strcmp(argv[i], "--") == 0) {
            first_kernel_argument = i + 1;
            break;
        }
        if (argv[i][0] != '-') {
            first_kernel_argument = i;
            break;
        }
        if (i + 1 >= argc) {
            usage(argv[0]);
            return 1;
        }
        if (strcmp(argv[i], "-k") == 0) {
            if (copy_path(kernel_path, sizeof(kernel_path), argv[i + 1]) < 0) {
                fprintf(stderr, "Kernel path is too long\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-r") == 0) {
            if (copy_path(rootfs_path, sizeof(rootfs_path), argv[i + 1]) < 0) {
                fprintf(stderr, "Root filesystem image path is too long\n");
                return 1;
            }
            rootfs_option = 1;
        } else if (strcmp(argv[i], "-f") == 0) {
            if (parse_memory_size(argv[i + 1], &free_memory) < 0) {
                fprintf(stderr, "Invalid free memory size: %s\n", argv[i + 1]);
                return 1;
            }
        } else {
            fprintf(stderr, "Unknown loader option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
        i += 2;
    }

    if (rootfs_option && full_path(rootfs_path, sizeof(rootfs_path)) < 0) {
        fprintf(stderr, "Failed to resolve root filesystem image path\n");
        return 1;
    }
    if (linux_path(rootfs_path, sizeof(rootfs_path)) < 0) {
        fprintf(stderr, "Linux root filesystem image path is too long\n");
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
    if (free_memory >= (uint32_t)maximum) {
        fprintf(stderr, "Not enough memory to leave %lu bytes free\n",
                (unsigned long)free_memory);
        return 1;
    }
    allocation_size = (uint32_t)maximum - free_memory;
    if (_dos_setblock(psp, allocation_size) < 0) {
        fprintf(stderr, "Failed to expand memory block\n");
        return 1;
    }

    /* Keep the complete loader allocation below the loaded kernel. */
    load_address = ((uintptr_t)program_end + LOAD_ALIGN - 1) &
                   ~(uintptr_t)(LOAD_ALIGN - 1);
    limit_address = (uintptr_t)psp + allocation_size;
    if (load_address >= limit_address) {
        fprintf(stderr, "No memory available for linux.sys\n");
        return 1;
    }

    if (build_command_line(&command_line, rootfs_path, limit_address,
                           argc, argv, first_kernel_argument) < 0) {
        fprintf(stderr, "Kernel command line is too long\n");
        return 1;
    }

    /* linux.sys contains an X executable despite its .sys extension. */
    result = _dos_loadonly((const char *)((uintptr_t)kernel_path | X_FILE_TYPE),
                           (void *)load_address, (void *)limit_address);
    if (result < 0) {
        fprintf(stderr, "Failed to load %s: %d\n", kernel_path, result);
        return 1;
    }

    start_kernel((void *)load_address, &command_line);
}
