/* Copyright  (C) 2010-2015 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (memmap.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdint.h>
#include <memmap.h>

#ifdef _WIN32
#define MAP_SHARED    0x01

#ifndef MAP_PRIVATE
#define MAP_PRIVATE   0x02
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

#define MAP_ANON      MAP_ANONYMOUS
#define MAP_FAILED    ((void *) -1)
#define PROT_READ     0x1
#define PROT_WRITE    0x2

/* This flag is only available in WinXP+ */
#ifdef FILE_MAP_EXECUTE
#ifndef PROT_EXEC
#define PROT_EXEC     0x4
#endif
#else
#ifndef PROT_EXEC
#define PROT_EXEC     0x0
#endif

#define FILE_MAP_EXECUTE 0
#endif

#ifdef __USE_FILE_OFFSET64
# define DWORD_HI(x) (x >> 32)
# define DWORD_LO(x) ((x) & 0xffffffff)
#else
# define DWORD_HI(x) (0)
# define DWORD_LO(x) (x)
#endif

void *mmap(void *start, size_t length, int prot, int flags, int fd, size_t offset)
{
	uint32_t flProtect, dwDesiredAccess;
	off_t end;
	HANDLE mmap_fd, h;
	void *ret;

	if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
		return MAP_FAILED;

	if (fd == -1)
   {
		if (!(flags & MAP_ANON) || offset)
			return MAP_FAILED;
	}
   else if (flags & MAP_ANON)
		return MAP_FAILED;

	if (prot & PROT_WRITE)
   {
      flProtect = PAGE_READWRITE;
      if (prot & PROT_EXEC)
         flProtect = PAGE_EXECUTE_READWRITE;
   }
   else if (prot & PROT_EXEC)
   {
      flProtect = PAGE_EXECUTE;
		if (prot & PROT_READ)
			flProtect = PAGE_EXECUTE_READ;
	}
   else
		flProtect = PAGE_READONLY;

	end = length + offset;

	if (fd == -1)
		mmap_fd = INVALID_HANDLE_VALUE;
	else
		mmap_fd = (HANDLE)_get_osfhandle(fd);

	h = CreateFileMapping(mmap_fd, NULL, flProtect, DWORD_HI(end), DWORD_LO(end), NULL);
	if (h == NULL)
		return MAP_FAILED;

   dwDesiredAccess = FILE_MAP_READ;
	if (prot & PROT_WRITE)
		dwDesiredAccess = FILE_MAP_WRITE;
	if (prot & PROT_EXEC)
		dwDesiredAccess |= FILE_MAP_EXECUTE;
	if (flags & MAP_PRIVATE)
		dwDesiredAccess |= FILE_MAP_COPY;
	ret = MapViewOfFile(h, dwDesiredAccess, DWORD_HI(offset), DWORD_LO(offset), length);

	if (ret == NULL)
   {
		CloseHandle(h);
		ret = MAP_FAILED;
	}

	return ret;
}

void munmap(void *addr, size_t length)
{
	UnmapViewOfFile(addr);
	/* ruh-ro, we leaked handle from CreateFileMapping() ... */
}

int mprotect(void *addr, size_t len, int prot)
{
   /* Incomplete, just assumes PAGE_EXECUTE_READWRITE right now
    * instead of correctly handling prot */
   prot = 0;
   if (prot & (PROT_READ | PROT_WRITE | PROT_EXEC))
      prot = PAGE_EXECUTE_READWRITE;
   return VirtualProtect(addr, len, prot, 0);
}

#elif !defined(HAVE_MMAN)
void* mmap(void *desired_addr, size_t len, int mmap_prot, int mmap_flags, int fildes, size_t off)
{
   return malloc(len);
}

void munmap(void *base_addr, size_t len)
{
   free(base_addr);
}

int mprotect(void *addr, size_t len, int prot)
{
   /* stub - not really needed at this point since this codepath has no dynarecs */
   return 0;
}

#endif

#if defined(__MACH__) && defined(__arm__)
#include <libkern/OSCacheControl.h>
#endif

int memsync(void *start, void *end)
{
   size_t len = (char*)end - (char*)start;
#if defined(__MACH__) && defined(__arm__)
   sys_dcache_flush(start ,len);
   sys_icache_invalidate(start, len);
   return 0;
#elif defined(__arm__) && !defined(__QNX__)
   (void)len;
   __clear_cache(start, end);
   return 0;
#elif defined(HAVE_MMAN)
   return msync(start, len, MS_SYNC | MS_INVALIDATE
#ifdef __QNX__
         MS_CACHE_ONLY
#endif
         );
#else
   (void)len;
   return 0;
#endif
}

int memprotect(void *addr, size_t len)
{
   return mprotect(addr, len, PROT_READ | PROT_WRITE | PROT_EXEC);
}
