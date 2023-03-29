#ifndef DMIDECODE_H
#define DMIDECODE_H

#include <sys/types.h>

struct dmi_data_t {
	char	bios_vendor[50];
	char	bios_version[50];
	char	bios_date[50];
	char	sys_vendor[50];
	char	sys_product[50];
	char	sys_version[50];
	char	sys_serial[50];
	char	board_vendor[50];
	char	board_product[50];
	char	board_version[50];
	char	board_serial[50];
};

typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned int u32;

/*
 * These macros help us solve problems on systems that don't support
 * non-aligned memory access. This isn't a big issue IMHO, since the tools
 * in this package are intended mainly for Intel and compatible systems,
 * which are little-endian and support non-aligned memory access. Anyway,
 * you may use the following defines to control the way it works:
 * - Define BIGENDIAN on big-endian systems.
 * - Define ALIGNMENT_WORKAROUND if your system doesn't support
 *   non-aligned memory access. In this case, we use a slower, but safer,
 *   memory access method.
 * You most probably will have to define none or the two of them.
 */

#ifdef BIGENDIAN
typedef struct {
	u32 h;
	u32 l;
} u64;
#else
typedef struct {
	u32 l;
	u32 h;
} u64;
#endif

#ifdef ALIGNMENT_WORKAROUND
static u64 U64(u32 low, u32 high)
{
	u64 self;
	
	self.l=low;
	self.h=high;
	
	return self;
}
#endif

#ifdef ALIGNMENT_WORKAROUND
#	ifdef BIGENDIAN
#	define WORD(x) (u16)((x)[1]+((x)[0]<<8))
#	define DWORD(x) (u32)((x)[3]+((x)[2]<<8)+((x)[1]<<16)+((x)[0]<<24))
#	define QWORD(x) (U64(DWORD(x+4), DWORD(x)))
#	else /* BIGENDIAN */
#	define WORD(x) (u16)((x)[0]+((x)[1]<<8))
#	define DWORD(x) (u32)((x)[0]+((x)[1]<<8)+((x)[2]<<16)+((x)[3]<<24))
#	define QWORD(x) (U64(DWORD(x), DWORD(x+4)))
#	endif /* BIGENDIAN */
#else /* ALIGNMENT_WORKAROUND */
#define WORD(x) (u16)(*(const u16 *)(x))
#define DWORD(x) (u32)(*(const u32 *)(x))
#define QWORD(x) (*(const u64 *)(x))
#endif /* ALIGNMENT_WORKAROUND */

int myread(int fd, u8 *buf, size_t count, const char *prefix);
int checksum(const u8 *buf, size_t len);
void *mem_chunk(off_t base, off_t len, const char *devmem);

#endif
