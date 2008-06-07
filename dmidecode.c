#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>


#include "util.h"
#include "dmidecode.h"

struct dmi_header
{
	u8 type;
	u8 length;
	u16 handle;
};


int myread(int fd, u8 *buf, size_t count, const char *prefix)
{
	ssize_t r=1;
	size_t r2=0;
	
	while(r2!=count && r!=0)
	{
		r=read(fd, buf+r2, count-r2);
		if(r==-1)
		{
			if(errno!=EINTR)
			{
				close(fd);
				perror(prefix);
				return -1;
			}
		}
		else
			r2+=r;
	}
	
	if(r2!=count)
	{
		close(fd);
		fprintf(stderr, "%s: Unexpected end of file\n", prefix);
		return -1;
	}
	
	return 0;
}

int checksum(const u8 *buf, size_t len)
{
	u8 sum=0;
	size_t a;
	
	for(a=0; a<len; a++)
		sum+=buf[a];
	return (sum==0);
}

/*
 * Copy a physical memory chunk into a memory buffer.
 * This function allocates memory.
 */
void *mem_chunk(off_t base, off_t len, const char *devmem)
{
	void *p;
	int fd;
	size_t mmoffset;
	void *mmp;
	
	if((fd=open(devmem, O_RDONLY))==-1)
	{
		perror(devmem);
		return NULL;
	}
	
	if((p=malloc(len))==NULL)
		return NULL;
	
	mmoffset=base%getpagesize();
	/*
	 * Please note that we don't use mmap() for performance reasons here,
	 * but to workaround problems many people encountered when trying
	 * to read from /dev/mem using regular read() calls.
	 */
	mmp=mmap(0, mmoffset+len, PROT_READ, MAP_SHARED, fd, base-mmoffset);
	if(mmp==MAP_FAILED)
	{
		fprintf(stderr, "%s: ", devmem);
		perror("mmap");
		free(p);
		return NULL;
	}
	
	memcpy(p, (u8 *)mmp+mmoffset, len);
	
	if(munmap(mmp, mmoffset+0x20)==-1)
	{
		fprintf(stderr, "%s: ", devmem);
		perror("munmap");
	}
	
	if(close(fd)==-1)
		perror(devmem);
	
	return p;
}
char *dmi_string(struct dmi_header *dm, u8 s)
{
	char *bp=(char *)dm;
	size_t i;

	if(s==0)
		return "Not Specified";
	
	bp+=dm->length;
	while(s>1 && *bp)
	{
		bp+=strlen(bp);
		bp++;
		s--;
	}
	
	if(!*bp)
		return "<bad index>";
	
	/* ASCII filtering */
	for(i=0; i<strlen(bp); i++)
		if(bp[i]<32 || bp[i]==127)
			bp[i]='.';
	
	return bp;
}

/*
 * Main
 */

static void dmi_decode(u8 *data, u16 ver)
{
	struct dmi_header *h=(struct dmi_header *)data;
	
	/*
	 * Note: DMI types 31, 37, 38 and 39 are untested
	 */
	switch(h->type)
	{
		case 0: /* 3.3.1 BIOS Information */
			if(h->length<0x12) break;

			snprintf(dmi_data.bios_vendor, sizeof(dmi_data.bios_vendor), "%s", dmi_string(h, data[0x04]));
			snprintf(dmi_data.bios_version, sizeof(dmi_data.bios_version), "%s", dmi_string(h, data[0x05]));
			snprintf(dmi_data.bios_date, sizeof(dmi_data.bios_date), "%s", dmi_string(h, data[0x06]));
			break;
		
		case 1: /* 3.3.2 System Information */
			if(h->length<0x08) break;

			snprintf(dmi_data.sys_vendor, sizeof(dmi_data.bios_vendor), "%s", dmi_string(h, data[0x04]));
			snprintf(dmi_data.sys_product, sizeof(dmi_data.bios_vendor), "%s", dmi_string(h, data[0x05]));
			snprintf(dmi_data.sys_version, sizeof(dmi_data.bios_vendor), "%s", dmi_string(h, data[0x06]));
			snprintf(dmi_data.sys_serial, sizeof(dmi_data.bios_vendor), "%s", dmi_string(h, data[0x07]));

			if(h->length<0x19) break;
			break;
		
		case 2: /* 3.3.3 Base Board Information */
			if(h->length<0x08) break;

			snprintf(dmi_data.board_vendor, sizeof(dmi_data.bios_vendor), "%s", dmi_string(h, data[0x04]));
			snprintf(dmi_data.board_product, sizeof(dmi_data.bios_vendor), "%s", dmi_string(h, data[0x05]));
			snprintf(dmi_data.board_version, sizeof(dmi_data.bios_vendor), "%s", dmi_string(h, data[0x06]));
			snprintf(dmi_data.board_serial, sizeof(dmi_data.bios_vendor), "%s", dmi_string(h, data[0x07]));
			break;
			
		default:
			break;
		}
		
		trim(dmi_data.board_vendor);
		trim(dmi_data.board_product);
		trim(dmi_data.bios_version);
}
		
static void dmi_table(u32 base, u16 len, u16 num, u16 ver, const char *devmem)
{
	u8 *buf;
	u8 *data;
	int i=0;
	
	if((buf=mem_chunk(base, len, devmem))==NULL)
	{
		return;
	}
	
	data=buf;
	while(i<num && data+sizeof(struct dmi_header)<=buf+len)
	{
		u8 *next;
		struct dmi_header *h=(struct dmi_header *)data;
		
		/* look for the next handle */
		next=data+h->length;
		while(next-buf+1<len && (next[0]!=0 || next[1]!=0))
			next++;
		next+=2;
		if(next-buf<=len)
			dmi_decode(data, ver);
		else
			printf("");
		
		data=next;
		i++;
	}

	free(buf);
}


static int smbios_decode(u8 *buf, const char *devmem)
{
	if(checksum(buf, buf[0x05])
	 && memcmp(buf+0x10, "_DMI_", 5)==0
	 && checksum(buf+0x10, 0x0F))
	{
		dmi_table(DWORD(buf+0x18), WORD(buf+0x16), WORD(buf+0x1C),
			(buf[0x06]<<8)+buf[0x07], devmem);
		return 1;
	}
	
	return 0;
}

int do_dmi_decode() {

	int found=0;
	off_t fp;
	const char *devmem="/dev/mem";
	u8 *buf;
	
	if((buf=mem_chunk(0xF0000, 0x10000, devmem))==NULL)
		exit(1);
	
	for(fp=0; fp<=0xFFF0; fp+=16)
	{
		if(memcmp(buf+fp, "_SM_", 4)==0 && fp<=0xFFE0)
		{
			if(smbios_decode(buf+fp, devmem))
				found++;
			fp+=16;
		}
	}
	
	free(buf);
}
