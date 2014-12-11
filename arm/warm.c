/*
 * wARM - exporting ARM processor specific privileged services to userspace
 * userspace part
 *
 * Copyright (c) Gražvydas "notaz" Ignotas, 2009
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the organization nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <errno.h>

#include "warm.h"

static void sys_cacheflush(void *start, void *end)
{
#ifdef __ARM_EABI__
	/* EABI version */
	int num = __ARM_NR_cacheflush;
	__asm__("mov  r0, %0 ;"
		"mov  r1, %1 ;"
		"mov  r2, #0 ;"
		"mov  r7, %2 ;"
		"swi  0" : : "r" (start), "r" (end), "r" (num)
			: "r0", "r1", "r2", "r3", "r7");
#else
	/* OABI */
	__asm__("mov  r0, %0 ;"
		"mov  r1, %1 ;"
		"mov  r2, #0 ;"
		"swi  %2" : : "r" (start), "r" (end), "i" __ARM_NR_cacheflush
			: "r0", "r1", "r2", "r3");
#endif
}

int warm_cache_op_range(int op, void *addr, unsigned long size)
{
   sys_cacheflush(addr, (char *)addr + size);
   return -1;
}
