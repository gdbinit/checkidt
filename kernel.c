/*
 *  _______  __                  __     ___  ______    _______
 * |   _   ||  |--..-----..----.|  |--.|   ||   _  \  |       |
 * |.  1___||     ||  -__||  __||    < |.  ||.  |   \ |.|   | |
 * |.  |___ |__|__||_____||____||__|__||.  ||.  |    \`-|.  |-'
 * |:  1   |                           |:  ||:  1    /  |:  |
 * |::.. . |                           |::.||::.. . /   |::.|
 * `-------'                           `---'`------'    `---'
 *
 * CheckIDT V1.2 - OS X version
 *
 * Based on kad's original code at Phrack #59
 * http://www.phrack.org/issues.html?issue=59&id=4#article
 *
 * (c) fG!, 2011, 2012, 2013, 2014 - reverser@put.as - http://reverse.put.as
 *
 * kernel.c
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

#include "kernel.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach/mach_vm.h>

#include "global.h"

/*
 * retrieve which kernel type are we running, 32 or 64 bits
 */
int32_t
get_kernel_type(void)
{
	size_t size = 0;
    int8_t ret = 0;
	sysctlbyname("hw.machine", NULL, &size, NULL, 0);
	char *machine = malloc(size);
	sysctlbyname("hw.machine", machine, &size, NULL, 0);
    
	if (strcmp(machine, "i386") == 0)
    {
		ret = X86;
    }
	else if (strcmp(machine, "x86_64") == 0)
    {
		ret = X64;
    }
	else
    {
		ret = -1;
    }
    
    free(machine);
    return ret;
}

/*
 * return values: same versioning used by Apple
 * 10 - Snow Leopard
 * 11 - Lion
 * 12 - Mountain Lion
 * 13 - Mavericks
 */
int32_t
get_kernel_version(void)
{
    int mib[2] = {0};
    size_t len = 0;
    char *kernelVersion = NULL;
    int32_t ret = 0;
    
    mib[0] = CTL_KERN;
    mib[1] = KERN_OSRELEASE;
    sysctl(mib, 2, NULL, &len, NULL, 0);
    kernelVersion = malloc(len * sizeof(char));
    sysctl(mib, 2, kernelVersion, &len, NULL, 0);
    
    if (strncmp(kernelVersion, "10.", 3) == 0)
    {
        ret = 10;
    }
    else if (strncmp(kernelVersion, "11.", 3) == 0)
    {
        ret = 11;
    }
    else if (strncmp(kernelVersion, "12.", 3) == 0)
    {
        ret = 12;
    }
    else if (strncmp(kernelVersion, "13.", 3) == 0)
    {
        ret = 13;
    }
    else
    {
        ret = -1;
    }
    free(kernelVersion);
    return ret;
}

/* from xnu/bsd/sys/kas_info.h */
#define KAS_INFO_KERNEL_TEXT_SLIDE_SELECTOR     (0)     /* returns uint64_t     */
#define KAS_INFO_MAX_SELECTOR           (1)

/*
 * inline asm to use the kas_info() syscall. beware the difference if we want 64bits syscalls!
 * alternative is to use: extern int kas_info(int selector, void *value, size_t *size)
 * doesn't need to be linked against any framework
 */
void
get_kaslr_slide(size_t *size, uint64_t *slide)
{
    // this is needed for 64bits syscalls!!!
    // good post about it http://thexploit.com/secdev/mac-os-x-64-bit-assembly-system-calls/
#define SYSCALL_CLASS_SHIFT                     24
#define SYSCALL_CLASS_MASK                      (0xFF << SYSCALL_CLASS_SHIFT)
#define SYSCALL_NUMBER_MASK                     (~SYSCALL_CLASS_MASK)
#define SYSCALL_CLASS_UNIX                      2
#define SYSCALL_CONSTRUCT_UNIX(syscall_number) \
((SYSCALL_CLASS_UNIX << SYSCALL_CLASS_SHIFT) | \
(SYSCALL_NUMBER_MASK & (syscall_number)))
    
    uint64_t syscallnr = SYSCALL_CONSTRUCT_UNIX(SYS_kas_info);
    uint64_t selector = KAS_INFO_KERNEL_TEXT_SLIDE_SELECTOR;
    int result = 0;
    __asm__ ("movq %1, %%rdi\n\t"
             "movq %2, %%rsi\n\t"
             "movq %3, %%rdx\n\t"
             "movq %4, %%rax\n\t"
             "syscall"
             : "=a" (result)
             : "r" (selector), "m" (slide), "m" (size), "a" (syscallnr)
             : "rdi", "rsi", "rdx", "rax"
             );
}

/* warning: caller must always supply the buffer since we are using mach_vm_read_overwrite */
kern_return_t
readkmem(struct config *cfg, void *buffer, mach_vm_address_t target_addr, const int read_size)
{
    if (cfg->kernel_port != 0)
    {
        mach_vm_size_t outsize = 0;
        kern_return_t kr = mach_vm_read_overwrite(cfg->kernel_port, target_addr, read_size, (mach_vm_address_t)buffer, &outsize);
        if (kr != KERN_SUCCESS)
        {
            ERROR_MSG("mach_vm_read_overwrite failed!");
            return KERN_FAILURE;
        }
    }
    else
    {
        if(lseek(cfg->fd_kmem, (off_t)target_addr, SEEK_SET) != (off_t)target_addr)
        {
            ERROR_MSG("Error in lseek. Are you root?");
            exit(-1);
        }
        if(read(cfg->fd_kmem, buffer, read_size) != read_size)
        {
            ERROR_MSG("Error while trying to read from kmem: %s.", strerror(errno));
            exit(-1);
        }
    }
    return KERN_SUCCESS;
}

void
writekmem(int fd, void *buffer, off_t offset, const int size)
{
	if(lseek(fd, offset, SEEK_SET) != offset)
	{
		ERROR_MSG("Error in lseek. Are you root?");
		exit(-1);
	}
	if(write(fd, buffer, size) != size)
	{
		ERROR_MSG("Error while trying to write to kmem: %s.", strerror(errno));
		exit(-1);
	}
}

void
retrieve_kernel_symbols(struct config *cfg)
{
    int kernel_fd = open("/mach_kernel", O_RDONLY);
    if (kernel_fd < 0)
    {
        ERROR_MSG("Failed to open /mach_kernel, %s.", strerror(errno));
        return;
    }
    struct stat stat = {0};
    if ( fstat(kernel_fd, &stat) < 0 )
    {
        ERROR_MSG("Can't fstat /mach_kernel, %s.", strerror(errno));
        close(kernel_fd);
        return;
    }
    uint8_t *kernel_buf = NULL;
    if ( (kernel_buf = mmap(0, stat.st_size, PROT_READ, MAP_SHARED, kernel_fd, 0)) == MAP_FAILED )
    {
        ERROR_MSG("mmap of /mach_kernel failed, %s.", strerror(errno));
        close(kernel_fd);
        return;
    }
    
    /* process the kernel mach-o header to find where symbols are */
    uint64_t linkedit_fileoff = 0;
    uint64_t linkedit_size = 0;
    uint32_t symboltable_fileoff = 0;
    uint32_t symboltable_nr_symbols = 0;
    uint32_t stringtable_fileoff = 0;
    uint32_t stringtable_size = 0;

    struct mach_header_64 *mh = (struct mach_header_64*)kernel_buf;
    /* test if it's a valid mach-o header (or appears to be) */
    if (mh->magic != MH_MAGIC_64)
    {
        ERROR_MSG("Target /mach_kernel is not 64 bits only!");
        return;
    }
    
    struct load_command *load_cmd = NULL;
    /* point to the first load command */
    char *load_cmd_addr = (char*)kernel_buf + sizeof(struct mach_header_64);
    /* iterate over all load cmds and retrieve required info to solve symbols */
    /* __LINKEDIT location and symbol/string table location */
    for (uint32_t i = 0; i < mh->ncmds; i++)
    {
        load_cmd = (struct load_command*)load_cmd_addr;
        if (load_cmd->cmd == LC_SEGMENT_64)
        {
            struct segment_command_64 *seg_cmd = (struct segment_command_64*)load_cmd;
            if (strncmp(seg_cmd->segname, "__LINKEDIT", 16) == 0)
            {
                linkedit_fileoff = seg_cmd->fileoff;
                linkedit_size    = seg_cmd->filesize;
            }
        }
        /* table information available at LC_SYMTAB command */
        else if (load_cmd->cmd == LC_SYMTAB)
        {
            struct symtab_command *symtab_cmd = (struct symtab_command*)load_cmd;
            symboltable_fileoff    = symtab_cmd->symoff;
            symboltable_nr_symbols = symtab_cmd->nsyms;
            stringtable_fileoff    = symtab_cmd->stroff;
            stringtable_size       = symtab_cmd->strsize;
        }
        load_cmd_addr += load_cmd->cmdsize;
    }
    
    /* pointer to __LINKEDIT offset */
    char *linkedit_buf = (char*)kernel_buf + linkedit_fileoff;
    
    /* retrieve all kernel symbols */
    SLIST_INIT(&cfg->symbols_head);
    struct nlist_64 *nlist = NULL;
    
    for (uint32_t i = 0; i < symboltable_nr_symbols; i++)
    {
        /* symbols and strings offsets into LINKEDIT */
        mach_vm_address_t symbol_off = symboltable_fileoff - linkedit_fileoff;
        mach_vm_address_t string_off = stringtable_fileoff - linkedit_fileoff;
        
        nlist = (struct nlist_64*)(linkedit_buf + symbol_off + i * sizeof(struct nlist_64));
        char *symbol_string = (linkedit_buf + string_off + nlist->n_un.n_strx);
        /* add symbols to linked list so we can search them later */
        struct symbols *new = malloc(sizeof(struct symbols));
        if (new != NULL)
        {
            new->address = nlist->n_value;
            new->name = symbol_string;
            SLIST_INSERT_HEAD(&cfg->symbols_head, new, entries);
        }
    }
}

void
resolve_symbol(struct config *cfg, mach_vm_address_t stub_addr, char *name, size_t name_size)
{
    int found = 0;
    struct symbols *el = NULL;
    SLIST_FOREACH(el, &cfg->symbols_head, entries)
    {
        /* the addresses we read from kernel memory are ASLRed so we need to fix it */
        if (stub_addr - cfg->kaslr_slide == el->address)
        {
            strncpy(name, el->name, name_size);
            found = 1;
            break;
        }
    }
    
    if (found == 0)
    {
        strncpy(name, "can't resolve", name_size);
    }
}
