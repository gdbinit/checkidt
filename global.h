/*
 *  _______  __                  __     ___  ______    _______
 * |   _   ||  |--..-----..----.|  |--.|   ||   _  \  |       |
 * |.  1___||     ||  -__||  __||    < |.  ||.  |   \ |.|   | |
 * |.  |___ |__|__||_____||____||__|__||.  ||.  |    \`-|.  |-'
 * |:  1   |                           |:  ||:  1    /  |:  |
 * |::.. . |                           |::.||::.. . /   |::.|
 * `-------'                           `---'`------'    `---'
 *
 * CheckIDT - OS X version
 *
 * Based on kad's original code at Phrack #59
 * http://www.phrack.org/issues.html?issue=59&id=4#article
 *
 * (c) fG!, 2011, 2012, 2013, 2014 - reverser@put.as - http://reverse.put.as
 *
 * global.h
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef checkidt_global_h
#define checkidt_global_h

#include <sys/param.h>
#include <sys/queue.h>

#define X86 0
#define X64 1

/*
 * Kernel descriptors for MACH - 64-bit flat address space.
 * @ osfmk/i386/seg.h
 */
#define KERNEL64_CS     0x08            /* 1:  K64 code */
#define SYSENTER_CS     0x0b            /*     U32 sysenter pseudo-segment */
#define KERNEL64_SS     0x10            /* 2:  KERNEL64_CS+8 for syscall */
#define USER_CS         0x1b            /* 3:  U32 code */
#define USER_DS         0x23            /* 4:  USER_CS+8 for sysret */
#define USER64_CS       0x2b            /* 5:  USER_CS+16 for sysret */
#define USER64_DS       USER_DS         /*     U64 data pseudo-segment */
#define KERNEL_LDT      0x30            /* 6:  */
/* 7:  other 8 bytes of KERNEL_LDT */
#define KERNEL_TSS      0x40            /* 8:  */
/* 9:  other 8 bytes of KERNEL_TSS */
#define KERNEL32_CS     0x50            /* 10: */
#define USER_LDT        0x58            /* 11: */
/* 12: other 8 bytes of USER_LDT */
#define KERNEL_DS       0x68            /* 13: 32-bit kernel data */
#define SYSENTER_TF_CS  (USER_CS|0x10000)
#define SYSENTER_DS     KERNEL64_SS     /* sysenter kernel data segment */

struct symbols
{
    uint64_t address;
    char *name;
    SLIST_ENTRY(symbols) entries;
};

struct config
{
    char in_filename[MAXPATHLEN];
    char out_filename[MAXPATHLEN];
    int interrupt;
    int read_file_archive;
    int create_file_archive;
    int compare_idt;
    int restore_idt;
    int show_all_descriptors;
    int resolve;
    int fd_kmem;
    int kernel_type;
    uint64_t kaslr_slide;
    size_t kaslr_size;
    uint64_t idt_addr;
    uint16_t idt_size;
    uint32_t idt_entries; /* nr of idt entries, should be always 256 */
    SLIST_HEAD(, symbols) symbols_head;
    mach_port_t kernel_port;
};

/* we only have 16 bytes descriptors because we are running in IA-32e mode! */
// 16 bytes IDT descriptor, used for 32 and 64 bits kernels (64 bit capable cpus!)
struct descriptor_idt
{
    uint16_t offset_low;
    uint16_t seg_selector;
    uint8_t reserved;
    uint8_t flag;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved2;
} __attribute__((packed));

/* logging macros */

#define ERROR_MSG(fmt, ...) fprintf(stderr, "[ERROR] " fmt " \n", ## __VA_ARGS__)
#define OUTPUT_MSG(fmt, ...) fprintf(stdout, fmt " \n", ## __VA_ARGS__)

#if DEBUG == 0
#   define DEBUG_MSG(fmt, ...) do {} while (0)
#else
#   define DEBUG_MSG(fmt, ...) fprintf(stdout, "[DEBUG] " fmt "\n", ## __VA_ARGS__)
#endif

#endif
