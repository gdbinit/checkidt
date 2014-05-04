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
 * idt.c
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

#include "idt.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/sysctl.h>
#include <errno.h>

#include "kernel.h"

/* local functions */
static char * get_segment(uint16_t selecteur);

/* retrieve the base address for the IDT */
mach_vm_address_t
get_addr_idt(int32_t kernel_type)
{
	// allocate enough space for 32 and 64 bits addresses
	uint8_t idtr[10] = {0};
	mach_vm_address_t idt = 0;
    
	__asm__ volatile ("sidt %0": "=m" (idtr));
	switch (kernel_type)
    {
		case X86:
			idt = *((uint32_t *)(idtr+2));
			break;
		case X64:
            idt = *(mach_vm_address_t *)(idtr+2);
			break;
		default:
			break;
	}
	return idt;
}

/* retrieve the size/limit for the IDT */
uint16_t
get_size_idt(void)
{
	// allocate enough space for 32 and 64 bits addresses
	uint8_t idtr[10] = {0};
	uint16_t size = 0;
	
	__asm__ volatile ("sidt %0": "=m" (idtr));
	size=*((uint16_t *) &idtr[0]);
	return size;
}


// FIXME
void
compare_idt(struct config *cfg)
{
	FILE *file_idt = NULL;
    int change=0;
    
	struct descriptor_idt save_descriptor = {0};
    struct descriptor_idt actual_descriptor = {0};
	unsigned long save_stub_addr = 0;
    unsigned long actual_stub_addr = 0;
	unsigned long high = 0;
    unsigned long middle = 0;
	   
	if ( (file_idt=fopen(cfg->in_filename, "r")) != 0 )
	{
		ERROR_MSG("Error while opening file %s.", cfg->in_filename);
		exit(-1);
	}
	
	for(int x = 0 ; x < 256; x++)
	{
		// read the descriptor from the filename
		fread(&save_descriptor, sizeof(struct descriptor_idt), 1, file_idt);
		switch (cfg->kernel_type)
        {
			case X86:
				save_stub_addr = (unsigned long)(save_descriptor.offset_middle << 16) + save_descriptor.offset_low;
				break;
			case X64:
				high = (unsigned long)save_descriptor.offset_high << 32;
				middle = (unsigned int)save_descriptor.offset_middle << 16;
				save_stub_addr = high + middle + save_descriptor.offset_low;
				break;
			default:
				break;
		}
		
		// read the descriptor from kernel memory
		readkmem(cfg, &actual_descriptor, cfg->idt_addr + 16*x, sizeof(struct descriptor_idt));
		switch (cfg->kernel_type)
        {
			case X86:
				actual_stub_addr = (unsigned long)(actual_descriptor.offset_middle << 16) + actual_descriptor.offset_low;
				break;
			case X64:
				high = (unsigned long)actual_descriptor.offset_high << 32;
				middle = (unsigned int)actual_descriptor.offset_middle << 16;
				actual_stub_addr = high + middle + actual_descriptor.offset_low;
				break;
			default:
				break;
		}
		
		// Houston, we have a problem!
		if(actual_stub_addr != save_stub_addr)
		{
			if(cfg->restore_idt == 0)
			{
				ERROR_MSG("Hey stub address of interrupt %i has changed!!!", x);
				ERROR_MSG("Old Value : 0x%.8lx.",save_stub_addr);
				ERROR_MSG("New Value : 0x%.8lx.",actual_stub_addr);
				change=1;
			}
			// FIXME
			else
			{
				ERROR_MSG("Restore old stub address of interrupt %i.", x);
				actual_descriptor.offset_high = (unsigned short) (save_stub_addr >> 16);
				actual_descriptor.offset_low  = (unsigned short) (save_stub_addr & 0x0000FFFF);
                //				actual_descriptor->offset = (unsigned long) (save_stub_addr);
				writekmem(cfg->fd_kmem, &actual_descriptor, cfg->idt_addr + 16*x, sizeof(struct descriptor_idt));
				change=1;
			}
		}
	}
	if(change == 0)
    {
		OUTPUT_MSG("[OK] All values for IDT descriptors are the same.");
    }
}

static char *
get_segment(uint16_t selecteur)
{
    switch (selecteur)
    {
        case KERNEL32_CS:
            return "KERNEL32_CS";
        case KERNEL_DS:
            return "KERNEL_DS";
        case KERNEL64_CS:
            return "KERNEL64_CS";
        default:
            return "UNKNOWN";
    }
}

void
show_idt_info(struct config *cfg)
{
	struct descriptor_idt descriptor = {0};
	unsigned long stub_addr = 0;
	mach_vm_address_t high = 0, middle = 0;
	uint16_t selecteur = 0;
	char type[15] = {0};
	char segment[15] = {0};
	char name[256] = {0};
	int x = 0;
	int dpl = 0;
    
	
	if(cfg->resolve == 1)
	{
        OUTPUT_MSG("* Interrupt  *    Stub Address    *   Segment   * DPL  *      Type       *     Handler Name     *");
		OUTPUT_MSG("-------------------------------------------------------------------------------------------------");
	}
	else
	{
        OUTPUT_MSG("* Interrupt  *    Stub Address    *   Segment   * DPL  *      Type      *");
		OUTPUT_MSG("-------------------------------------------------------------------------");
	}
	
	if(cfg->interrupt != 0)
	{
		readkmem(cfg, &descriptor, cfg->idt_addr + 16*cfg->interrupt, sizeof(struct descriptor_idt));
		switch (cfg->kernel_type)
        {
			case X86:
				stub_addr=(unsigned long)(descriptor.offset_middle << 16) + descriptor.offset_low;
				break;
			case X64:
				high = (mach_vm_address_t)descriptor.offset_high << 32;
				middle = descriptor.offset_middle << 16;
				stub_addr = high + middle + descriptor.offset_low;
				break;
			default:
				break;
		}
        
		selecteur=(unsigned short)descriptor.seg_selector;
        /* which privilege level can access the interrupt, ring 3 or 0 */
		if(descriptor.flag & 0x60)
        {
			dpl = 3; /* ring 3 ok */
        }
		else
        {
			dpl = 0; /* only ring 0 */
		}
        /* gate type */
        switch (descriptor.flag & 0xF)
        {
            case 0x5:
                strcpy(type, "Task gate");
                break;
                /* only this is set in OS X... */
            case 0xE:
                strcpy(type, "Interrupt gate");
                break;
            case 0xF:
                strcpy(type, "Trap gate");
                break;
            default:
                strcpy(type, "Unknown");
                break;
        }
        
		strcpy(segment, get_segment(selecteur));
		if(cfg->resolve == 1)
		{
			resolve_symbol(cfg, stub_addr, name, sizeof(name));
			OUTPUT_MSG("      0x%-4x   0x%-16lx   %-12s   %-3i   %-16s  %s", cfg->interrupt, stub_addr, segment, dpl, type, name);
		}
		else
		{
			OUTPUT_MSG("      0x%-4x   0x%-16lx   %-12s   %-3i   %s", cfg->interrupt, stub_addr, segment, dpl, type);
		}
	}
	if(cfg->show_all_descriptors == 1 )
	{
		for (x = 0; x < cfg->idt_entries; x++)
		{
			readkmem(cfg, &descriptor, cfg->idt_addr + 16*x, sizeof(struct descriptor_idt));
            
			switch (cfg->kernel_type)
            {
				case X86:
					stub_addr=(unsigned long)(descriptor.offset_middle << 16) + descriptor.offset_low;
					break;
				case X64:
                    high = (unsigned long)descriptor.offset_high << 32;
                    middle = descriptor.offset_middle << 16;
					stub_addr = high + middle + descriptor.offset_low;
					break;
				default:
					break;
			}
			
			if(stub_addr != 0)
			{
				selecteur=(uint16_t) descriptor.seg_selector;
				if(descriptor.flag & 0x60)
                {
					dpl = 3;
                }
				else
                {
					dpl = 0;
				}
                switch (descriptor.flag & 0xF)
                {
                    case 0x5:
                        strcpy(type, "Task gate");
                        break;
                        /* only this is set in OS X... */
                    case 0xE:
                        strcpy(type, "Interrupt gate");
                        break;
                    case 0xF:
                        strcpy(type, "Trap gate");
                        break;
                    default:
                        strcpy(type, "Unknown");
                        break;
                }
                
				strcpy(segment, get_segment(selecteur));
				if(cfg->resolve == 1)
				{
					resolve_symbol(cfg, stub_addr, name, sizeof(name));
					OUTPUT_MSG("      0x%-4x   0x%-16lx   %-12s   %-3i   %-16s  %s", x, stub_addr, segment, dpl, type, name);
				}
				else
				{
					OUTPUT_MSG("      0x%-4x   0x%-16lx   %-12s   %-3i   %s", x, stub_addr, segment, dpl, type);
				}
			}
		}
	}
}

void
create_idt_archive(struct config *cfg)
{
	FILE *file_idt = NULL;
	struct descriptor_idt descriptor = {0};
    
	if ( (file_idt = fopen(cfg->out_filename, "w")) == NULL )
	{
		ERROR_MSG("Error while opening file %s, %s.", cfg->out_filename, strerror(errno));
		exit(-1);
	}
	for (uint32_t x = 0; x < cfg->idt_entries; x++)
	{
		readkmem(cfg, &descriptor, cfg->idt_addr + 16*x, sizeof(struct descriptor_idt));
		fwrite(&descriptor, sizeof(struct descriptor_idt), 1, file_idt);
	}
	fclose(file_idt);
    OUTPUT_MSG("[OK] Creating file archive idt done");
}

/* FIXME */
void
read_idt_archive(struct config *cfg)
{
	FILE *file_idt = NULL;
	struct descriptor_idt descriptor  = {0};
	unsigned long stub_addr = 0;
	unsigned long high = 0;
    unsigned long middle = 0;
	
	if ( (file_idt=fopen(cfg->in_filename, "r")) == NULL )
	{
		ERROR_MSG("Target file %s does not exist.",cfg->in_filename);
		exit(-1);
	}
    
	for (uint32_t x = 0; x < cfg->idt_entries; x++)
	{
		fread(&descriptor, sizeof(struct descriptor_idt), 1, file_idt);
		switch (cfg->kernel_type)
        {
			case X86:
				stub_addr=(unsigned long)(descriptor.offset_middle << 16) + descriptor.offset_low;
				break;
			case X64:
				high = (unsigned long)descriptor.offset_high << 32;
				middle = (unsigned int)descriptor.offset_middle << 16;
				stub_addr = high + middle + descriptor.offset_low;
				break;
			default:
				break;
		}
		printf("Interrupt: %-3i  -- Stub address: 0x%.8lx\n", x, stub_addr);
	}
	fclose(file_idt);
}
