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
 * Note: This requires kmem/mem devices to be enabled
 * Edit /Library/Preferences/SystemConfiguration/com.apple.Boot.plist
 * add kmem=1 parameter, and reboot!
 *
 * v1.0 - Initial port to OS X - almost everything is working :-)
 * v1.1 - Adding a new option -k to calculate sysent address
 *		  This might fail due to small opcodes differences
 *        Disassembly is probably a much better method than this
 *        It's just a PoC :-)
 * v1.2 - Minor cleanups
 * v2.0 - Major code cleanup and update to latest OS X versions
 *        Remove the sysent option
 *        Retrieve kernel symbols from running kernel
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "global.h"
#include "kernel.h"
#include "idt.h"

#define VERSION "2.0"

static void
usage(void)
{
	fprintf(stderr,"Available Options : \n");
	fprintf(stderr,"       -a int_nr show all info about one interrupt\n");
	fprintf(stderr,"       -A        showw all info about all interrupts\n");
	fprintf(stderr,"       -c        create file archive\n");
	fprintf(stderr,"       -r        read file archive\n");
	fprintf(stderr,"       -o file   output filename (for creating file archive)\n");
	fprintf(stderr,"       -C        compare save idt & new idt\n");
	fprintf(stderr,"       -R        restore IDT\n");
	fprintf(stderr,"       -i file   input filename to compare or read\n");
	fprintf(stderr,"       -s        resolve symbols\n");
	exit(1);
}

static void
header(void)
{
    OUTPUT_MSG("  _______  __                  __     ___  ______    _______ ");
    OUTPUT_MSG(" |   _   ||  |--..-----..----.|  |--.|   ||   _  \\  |       |");
    OUTPUT_MSG(" |.  1___||     ||  -__||  __||    < |.  ||.  |   \\ |.|   | |");
    OUTPUT_MSG(" |.  |___ |__|__||_____||____||__|__||.  ||.  |    \\`-|.  |-'");
    OUTPUT_MSG(" |:  1   |                           |:  ||:  1    /  |:  |  ");
    OUTPUT_MSG(" |::.. . |                           |::.||::.. . /   |::.|  ");
    OUTPUT_MSG(" `-------'                           `---'`------'    `---'  ");
	OUTPUT_MSG("   CheckIDT v%s - OS X version by fG! (original by kad)",VERSION);
	OUTPUT_MSG("   -----------------------------------------------------");
}

int
main(int argc, char ** argv)
{
	int option = 0;
	struct config cfg = {0};

	header();
	if (argc < 2)
	{
		usage();
	}
		
	while( (option=getopt(argc,argv,"ha:Aco:Ci:rRs")) != -1 )
	{
		switch(option)
		{
			case 'h': 
				usage();
				exit(1);
			case 'a':
				cfg.interrupt = atoi(optarg);
				break;
			case 'A': 
				cfg.show_all_descriptors = 1;
				break;
			case 'c': 
				cfg.create_file_archive = 1;
				break;
			case 'r': 
				cfg.read_file_archive = 1;
				break;
			case 'R': 
				cfg.restore_idt = 1;
				break;
			case 'o':
				if(strlen(optarg) > MAXPATHLEN - 1)
				{
					ERROR_MSG("File name too long.");
					return -1;
				}
				strncpy(cfg.out_filename, optarg, sizeof(cfg.out_filename));
				break;
			case 'C': 
				cfg.compare_idt = 1;
				break;
			case 'i': 
				if(strlen(optarg) > MAXPATHLEN - 1)
				{
					ERROR_MSG("File name too long.");
					return -1;
				}
				strncpy(cfg.in_filename, optarg, sizeof(cfg.in_filename));
				break;
			case 's': 
				cfg.resolve = 1;
				break;
		}
	}
	OUTPUT_MSG("");
	
	cfg.kernel_type = get_kernel_type();
	if (cfg.kernel_type == -1)
	{
		ERROR_MSG("Unable to retrieve kernel type.");
		return -1;
	}
    else if (cfg.kernel_type == X86)
    {
        ERROR_MSG("32 bits kernels not supported.");
        return -1;
    }
    
	cfg.idt_addr = get_addr_idt(cfg.kernel_type);
	cfg.idt_size = get_size_idt();
    cfg.idt_entries = cfg.idt_size / sizeof(struct descriptor_idt);
    /* we need to populate the size variable else syscall fails */
    cfg.kaslr_size = sizeof(cfg.kaslr_size);
    get_kaslr_slide(&cfg.kaslr_size, &cfg.kaslr_slide);
    
    OUTPUT_MSG("[INFO] Kaslr slide is 0x%llx", cfg.kaslr_slide);
    OUTPUT_MSG("[INFO] IDT base address is: 0x%llx", cfg.idt_addr);
    OUTPUT_MSG("[INFO] IDT size: 0x%x\n", cfg.idt_size);
    
	if( (cfg.fd_kmem = open("/dev/kmem",O_RDWR)) == -1 )
	{
		ERROR_MSG("Error while opening /dev/kmem. Is /dev/kmem enabled?");
        ERROR_MSG("Verify that /Library/Preferences/SystemConfiguration/com.apple.Boot.plist has kmem=1 parameter configured.");
		return -1;
	}
    
    if (cfg.resolve == 1)
    {
        retrieve_kernel_symbols(&cfg);
    }
    
    if(cfg.interrupt >= 0 || cfg.show_all_descriptors == 1)
	{
		show_idt_info(&cfg);
	}
	if(cfg.create_file_archive == 1)
	{
		create_idt_archive(&cfg);
	}
	if(cfg.read_file_archive == 1)
	{
		read_idt_archive(&cfg);
	}
	if(cfg.compare_idt == 1)
	{
		compare_idt(&cfg);
	}
	if(cfg.restore_idt == 1)
	{
		compare_idt(&cfg);
	}
	return 0;
}
