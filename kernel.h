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
 * kernel.h
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

#ifndef checkidt_kernel_h
#define checkidt_kernel_h

#include <sys/types.h>
#include <stdint.h>
#include <mach/mach.h>
#include "global.h"

/* exported functions */
int32_t get_kernel_type (void);
int32_t get_kernel_version(void);
void get_kaslr_slide(size_t *size, uint64_t *slide);
kern_return_t readkmem(struct config *cfg, void *buffer, mach_vm_address_t target_addr, const int read_size);
void writekmem(int fd, void *buffer, off_t offset, const int size);
void retrieve_kernel_symbols(struct config *cfg);
void resolve_symbol(struct config *cfg, mach_vm_address_t stub_addr, char *name, size_t name_size);

#endif
