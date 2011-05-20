/*
  Copyright (C) <2011> Julian Chesterfield <jac90@cl.cam.ac.uk>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Library General Public License as
  published by the Free Software Foundation, version 2.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
  USA
*/

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <syslog.h>
#define DPRINTF(_f, _a...) syslog(LOG_ERR, _f, ##_a)

#define roundup(x,n) (((x)+((n)-1))&(~((n)-1)))
#define rounddown(x,n) (((x)/(n))*(n))

#define SECTOR_SIZE 1 << 9
#define PAGE_SIZE SECTOR_SIZE << 3
#define START_OFFSET PAGE_SIZE << 10
#define BLOCKSTORE_SIZE PAGE_SIZE << 3
#define DEFAULT_BLOCK_OFFSET 64 + 1 /*Add 1 page for the BS header*/
#define MAX_FSTABLE_ENTRIES 400

#define SIMPLE_MAGIC_HDR 0xF0F0
#define FSLOOKUP_TABLE_HDR 0xCFCF
#define BLOCKSUM_MAGIC_HDR 0x0F0F
#define BLOCKSTORE_MAGIC_HDR 0xAFAF
#define BLOCKSTORE_MAGIC_HASHPAGE 0xBFBF
/*typedef struct
{
  char d[8];
  } u_int128_t;*/

typedef struct 
{
	u_int32_t digest[ 5 ];            /* Message digest */
	u_int32_t countLo, countHi;       /* 64-bit bit count */
	u_int32_t data[ 16 ];             /* SHS data buffer */
	int Endianness;
} SHA_CTX;

void SHAInit(SHA_CTX *);
void SHAUpdate(SHA_CTX *, unsigned char *buffer, int count);
void SHAFinal(unsigned char *output, SHA_CTX *);

struct fs_hdr {
  u_int16_t magic;
  u_int64_t offset;
  u_int64_t length;
  u_int32_t mode;
  char filename[486];
} __attribute__((__packed__));

struct fs_lookup_table {
  u_int16_t magic;
  u_int32_t next_offset;
} __attribute__((__packed__));

struct table_mask_entry {
  u_int16_t sh;
  unsigned char lh[8];
} __attribute__((__packed__));

struct blockstore_hdr {
  u_int16_t magic;
  u_int16_t bslt_offset;
  u_int32_t next_block;
} __attribute__((__packed__));

struct blockstore_hashpage_hdr {
  u_int16_t magic;
  u_int16_t sh;
  u_int16_t entries;
} __attribute__((__packed__));

struct blockstore_hashpage_entry {
  char lh[8];
  u_int32_t offset;
} __attribute__((__packed__));

struct fs_ctx {
  struct fs_lookup_table *fslt;
  u_int32_t *bslt;
  int fsfd;
  int bsfd;
  char *FSMDlookup;
  char *fslt_mask_cache;
  char *bsentry_cache;
  char *block_cache;
  u_int32_t next_block;
  u_int32_t next_fs_block;
  SHA_CTX sha;
};

#define MAX_HASHPAGE_ENTRIES (PAGE_SIZE/sizeof(struct blockstore_hashpage_entry)) - 1
#define MAX_FS_LOOKUP_ENTRIES (PAGE_SIZE-sizeof(struct fs_lookup_table))/sizeof(struct table_mask_entry)

struct fs_hdr *init_hdr(char *filename, int length, u_long offset, u_short magic);
struct fs_hdr *read_hdr(int fd, u_short magic);
int fcopy(int infd, int outfd, u_long length);
u_int32_t fcopy_to_blockstore(int infd, struct fs_ctx *fsc, u_int32_t length);
int fcopy_from_blockstore(int outfd, u_int32_t offset, struct fs_ctx *fsc, u_int32_t length);
int fmetadata_init(int outfd, u_int32_t offset, struct fs_ctx *fsc, u_int32_t length);
u_int16_t gen_shorthash(char *octets);
unsigned char *gen_longhash(char *octets, SHA_CTX *sha);
struct fs_ctx *init_fs_ctx(void);
int init_sha1(struct fs_ctx *fsc);
int init_blockstore(char *bsdev, struct fs_ctx *fsc);
void close_blockstore(struct fs_ctx *fsc);
struct fs_hdr *init_hdr(char *filename, int length, u_long offset, u_short magic);
int write_block(struct fs_ctx *fsc, char *buf, struct table_mask_entry *tme);
int read_block(struct fs_ctx *fsc, char *buf, u_int16_t sh, unsigned char *lh);
