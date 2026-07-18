/* Linux kernel loader for X68000 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <x68k/iocs.h>
#include <x68k/dos.h>

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
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
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
    fclose(fp);

    // Initialize screen
    _iocs_crtmod(12);
    _iocs_g_clr_on();
    _iocs_b_curoff();

    _dos_super(0);

    __asm__ volatile ("ori.w #0x0700,%sr");

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
