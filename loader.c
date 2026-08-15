/* Linux kernel loader for X68000 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <x68k/iocs.h>
#include <x68k/dos.h>
#include "puff.h"

static uint32_t get_le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t crc32(const unsigned char *data, size_t size)
{
    uint32_t crc = ~0U;

    while (size--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xedb88320U & -(crc & 1));
    }
    return ~crc;
}

static int skip_gzip_string(const unsigned char *data, size_t size,
                            size_t *offset)
{
    while (*offset < size && data[(*offset)++] != 0)
        ;
    return *offset <= size && data[*offset - 1] == 0;
}

static void gunzip_progress(unsigned long done, unsigned long total)
{
    (void)done;
    (void)total;
    putchar('.');
    fflush(stdout);
}

static unsigned char *gunzip(const unsigned char *data, size_t size,
                             size_t *output_size)
{
    size_t offset = 10;
    unsigned char flags;
    unsigned long source_len, dest_len;
    unsigned char *output;

    if (size < 18 || data[0] != 0x1f || data[1] != 0x8b || data[2] != 8)
        return NULL;
    flags = data[3];
    if (flags & 0xe0)
        return NULL;
    if (flags & 4) {
        unsigned int extra_len;
        if (offset + 2 > size - 8)
            return NULL;
        extra_len = data[offset] | (unsigned int)data[offset + 1] << 8;
        offset += 2;
        if (extra_len > size - 8 - offset)
            return NULL;
        offset += extra_len;
    }
    if ((flags & 8) && !skip_gzip_string(data, size - 8, &offset))
        return NULL;
    if ((flags & 16) && !skip_gzip_string(data, size - 8, &offset))
        return NULL;
    if (flags & 2) {
        if (offset + 2 > size - 8)
            return NULL;
        offset += 2;
    }
    if (offset > size - 8)
        return NULL;

    dest_len = get_le32(data + size - 4);
    if (dest_len == 0)
        return NULL;
    output = malloc(dest_len);
    if (output == NULL)
        return NULL;

    source_len = size - 8 - offset;
    if (puff(output, &dest_len, data + offset, &source_len,
             gunzip_progress) != 0 ||
        source_len != size - 8 - offset ||
        dest_len != get_le32(data + size - 4) ||
        crc32(output, dest_len) != get_le32(data + size - 8)) {
        free(output);
        return NULL;
    }
    *output_size = dest_len;
    return output;
}

static int is_x68000z(void)
{
    volatile unsigned char *system_port = (volatile unsigned char *)0xe8e000;

    /* X68000Z version query */
    *system_port = 'Z';
    return *system_port == 'X';
}

void startlinux(void *addr, void *data, size_t size)
{
    register int a0 __asm__("a0") = (int)addr;
    register int a1 __asm__("a1") = (int)data;
    register int d0 __asm__("d0") = (int)size;
    __asm__ volatile (
        "   lea.l %%pc@(execute),%%a3\n"
        "   lea.l 0x3800,%%a4\n"
        "   lea.l %%pc@(execute_end),%%a5\n"
        "1:\n"
        "   move.w %%a3@+,%%a4@+\n"
        "   cmpa.l %%a5,%%a3\n"
        "   bne.s 1b\n"
        "   jmp 0x3800\n"

        "execute:\n"
        "   move.l %%a0,%%a2\n"
        "1:\n"
        "   move.w %%a1@+,%%a0@+\n"
        "   subq.l #2,%%d0\n"
        "   bgt.s 1b\n"
        "   jmp %%a2@\n"
        "execute_end:\n"
        : : "a"(a0), "a"(a1), "d"(d0)
    );
}

int main(int argc, char **argv)
{
    char *vmlinux = "vmlinux.bin";

    if (argc > 1) {
        vmlinux = argv[1];
    }

    // Load the Linux kernel binary into memory
    FILE *fp = fopen(vmlinux, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open %s\n", vmlinux);
        return 1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "Failed to seek %s\n", vmlinux);
        fclose(fp);
        return 1;
    }
    long file_size = ftell(fp);
    if (file_size < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Failed to determine size of %s\n", vmlinux);
        fclose(fp);
        return 1;
    }
    size_t size = file_size;
    char *data = (char *)malloc(size);
    if (data == NULL) {
        fprintf(stderr, "Failed to allocate memory for %s\n", vmlinux);
        fclose(fp);
        return 1;
    }
    printf("Loading %s", vmlinux);
    char *p = data;
    size_t len;
    while ((len = fread(p, 1, 65536, fp)) > 0) {
        p += len;
        printf(".");
        fflush(stdout);
    }
    printf("\n");
    if (ferror(fp)) {
        fprintf(stderr, "Failed to read %s\n", vmlinux);
        free(data);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    if (size >= 2 && (unsigned char)data[0] == 0x1f &&
        (unsigned char)data[1] == 0x8b) {
        size_t uncompressed_size;
        unsigned char *uncompressed;

        printf("Decompressing %s", vmlinux);
        fflush(stdout);
        uncompressed = gunzip((const unsigned char *)data, size,
                              &uncompressed_size);
        if (uncompressed == NULL) {
            fprintf(stderr, "\nFailed to decompress %s\n", vmlinux);
            free(data);
            return 1;
        }
        printf(" done\n");
        free(data);
        data = (char *)uncompressed;
        size = uncompressed_size;
    }

    _iocs_b_curoff();

    int mode = _iocs_set232c(-1);

    _dos_super(0);
    __asm__ volatile ("ori.w #0x0700,%sr");

    if (!is_x68000z()) {    // Workaround for X68000 Z
        __asm__ volatile ("reset");
    }

    // Initialize MFP
    static const unsigned char mfp_init[] = {
        0x05, 0x00,  0x03, 0x06,  0x1d, 0x70,  0x23, 0xc8,
        0x07, 0x18,  0x09, 0x26,  0x13, 0x18,  0x15, 0x26,
        0x17, 0x40,  0x19, 0x08,  0x1b, 0x01,  0x21, 0x0d,
        0x29, 0x88,  0x2b, 0x01,  0x2f, 0xff,  0x2d, 0x01,
    };
    for (int i = 0; i < sizeof(mfp_init); i += 2) {
        *(volatile unsigned char *)(0xe88000 + mfp_init[i]) =
            mfp_init[i + 1];
    }

    // Initialize SCC
    static const unsigned char scca_init[] = {
        0x09, 0xc0,  0x09, 0x80,  0x04, 0x45,  0x01, 0x00,
        0x02, 0x50,  0x03, 0xc0,  0x05, 0xe2,  0x09, 0x01,
        0x0b, 0x56,  0x0c, 0x0e,  0x0d, 0x00,  0x0e, 0x02,
        0x03, 0xc1,  0x05, 0xea,  0x00, 0x80,  0x0e, 0x03,
        0x0f, 0x00,  0x00, 0x10,  0x00, 0x10,  0x01, 0x10,
    };
    for (int i = 0; i < sizeof(scca_init); i++) {
        *(volatile unsigned char *)0xe98005 = scca_init[i];
    }

    static const unsigned char sccb_init[] = {
        0x09, 0x40,  0x04, 0x4c,  0x01, 0x00,  0x03, 0xc0,
        0x05, 0x60,  0x0b, 0x56,  0x0c, 0x1f,  0x0d, 0x00,
        0x0e, 0x02,  0x03, 0xc1,  0x05, 0xe8,  0x00, 0x80,
        0x0e, 0x03,  0x0f, 0x00,  0x00, 0x10,  0x00, 0x10,
        0x01, 0x10,  0x09, 0x09,
    };
    for (int i = 0; i < sizeof(sccb_init); i++) {
        *(volatile unsigned char *)0xe98003 = sccb_init[i];
    }

    // Set serial port mode
    _iocs_set232c(mode);

    // Initialize screen
#ifdef CONFIG_FB_SIMPLE
    _iocs_crtmod(12);
    _iocs_g_clr_on();
#else
    _iocs_crtmod(16);
    for (int i = 0; i < 16; i++) {
        // Convert VGA color code (0-15) to GGGGGRRRRRBBBBBI format (16-bit)
        int r = (i & 0x1) ? 0x1f : 0x00;  // Red
        int g = (i & 0x2) ? 0x1f : 0x00;  // Green
        int b = (i & 0x4) ? 0x1f : 0x00;  // Blue
        int bright = (i & 0x8) ? 0x01 : 0x00;  // Intensity
        int color = (g << 11) | (r << 6) | (b << 1) | bright;
        _iocs_tpalet2(i, color);
    }
#endif

    // Set graphics color palette
    for (int i = 0; i < 128; i++) {
        *(volatile unsigned char *)(0xe82000 + i * 4) = (i * 2) & 0x38;
        *(volatile unsigned char *)(0xe82001 + i * 4) = (i * 2 + 1) & 0x38;
        *(volatile unsigned char *)(0xe82002 + i * 4) = (((i * 2) & 0xe0) >> 5) | (((i * 2) & 0x07) << 5);
        *(volatile unsigned char *)(0xe82003 + i * 4) = (((i * 2 + 1) & 0xe0) >> 5) | (((i * 2 + 1) & 0x07) << 5);
    }

    // Supervisor area set
    *(volatile unsigned char *)0xe86001 = 0;

    // MFP interrupt mask
    *(volatile unsigned char *)0xe88007 = 0;
    *(volatile unsigned char *)0xe88009 = 0;

    // SCC interrupt mask
    *(volatile unsigned char *)0xe98001 = 9;
    *(volatile unsigned char *)0xe98001 = 0;

    // IOC interrupt mask
    *(volatile unsigned char *)0xe9c001 = 0;

    // Jump to Linux kernel entry point
    startlinux((void *)0x004000, data, size);

    return 0;
}
