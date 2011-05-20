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


#include "hashfs.h"

void printhex(unsigned char *ptr, int len) {
  int i;

  for(i=0;i<len;i++)
    DPRINTF("%02x",ptr[i]);

  DPRINTF("\n");
}

int bytecompare(char *src, char *dst, int len) {
  int i;

  for(i=0;i<len;i++)
    if(src[i] != dst[i])
      return 0;
  return 1;
}

/*
 * HashFs Blocksum utilities
 */
struct fs_ctx *init_fs_ctx(void) {
	return (struct fs_ctx *)malloc(sizeof(struct fs_ctx));
}

int init_blockstore(char *bsdev, struct fs_ctx *fsc) {
  fsc->next_block = DEFAULT_BLOCK_OFFSET;

  if (init_blockstore_fd(fsc, bsdev) <= 0) {
    DPRINTF("Failed to open filehandle: %s\n",bsdev);
    return -1;
  }

  //Check to see if blockstore initialised
  if(init_blockstore_header(fsc)==-1) {
    create_blockstore_header(fsc);
    create_lookup_table(fsc);
    init_blockstore_header(fsc);
  } else
    DPRINTF("Blockstore header initialised\n");

  init_lookup_table(fsc);
  return 1;
}

void debug_bslt(struct fs_ctx *fsc) {
  int i;

  DPRINTF("Reading BS Lookup table:\n");
  for(i=0;i<0xFFFF;i++) {
    if (fsc->bslt[i] != 0) DPRINTF("%d: %d\n",i,fsc->bslt[i]);
  }  
  DPRINTF("DONE\n");
}

void close_blockstore(struct fs_ctx *fsc) {
  create_blockstore_header(fsc);
  close(fsc->bsfd);
  //debug_bslt(fsc);
  free(fsc->bslt);
  free(fsc->fslt);
  free(fsc);
}

int init_blockstore_fd(struct fs_ctx *fsc, char *blockstore_dev) {
  fsc->bsfd = open(blockstore_dev, O_RDWR);
  return fsc->bsfd;
}

int init_sha1(struct fs_ctx *fsc) {
  SHAInit(&fsc->sha);
}

int init_lookup_table(struct fs_ctx *fsc) {
  int len = 0;
  char *p;
  int size = (0xFFFF)*4;

  p = calloc(1, size);
  /*Read in the blockstore lookup table*/
  lseek(fsc->bsfd, PAGE_SIZE, SEEK_SET);
  while(len<size)
    len += read(fsc->bsfd, p + len, size-len);

  fsc->bslt = (u_int32_t *)p;
  //debug_bslt(fsc);
  return 1;
}

int init_blockstore_header(struct fs_ctx *fsc) {
  char *p = malloc(PAGE_SIZE);
  struct blockstore_hdr *bh = (struct blockstore_hdr *)p;
  int len = 0;

  /*Read in the blockstore header*/
  lseek(fsc->bsfd, 0, SEEK_SET);
  while(len<PAGE_SIZE)
    len += read(fsc->bsfd, p + len, PAGE_SIZE-len);

  if (bh->magic != BLOCKSTORE_MAGIC_HDR) {
    DPRINTF("Not a valid Blockstore target: [%x,%x]\n",bh->magic,BLOCKSTORE_MAGIC_HDR);
    free(p);
    return -1;
  }
  DPRINTF("Initialised valid Blockstore target\n");
  fsc->next_block = bh->next_block;
  free(p);
  return 1;
}

int create_blockstore_header(struct fs_ctx *fsc) {
  struct blockstore_hdr *bh;
  int len = 0;
  char *p = malloc(PAGE_SIZE);

  bh = (struct blockstore_hdr *)p;
  bh->magic = BLOCKSTORE_MAGIC_HDR;
  bh->bslt_offset = 1; //Page sized units
  bh->next_block = fsc->next_block; //32k for bs Lookup table + Header page

  /*Write the blockstore header*/
  lseek(fsc->bsfd, 0, SEEK_SET);
  while(len<PAGE_SIZE)
    len += write(fsc->bsfd, p + len, PAGE_SIZE-len);

  free(p);
  return 1;
}

int create_lookup_table(struct fs_ctx *fsc) {
  int len = 0, i;
  int size = (0xFFFF)*4;
  char *page = calloc(1, PAGE_SIZE);

  lseek(fsc->bsfd, PAGE_SIZE, SEEK_SET);
  for(i=0; i<(size>>12); i++) {
    len = 0;
    while(len<PAGE_SIZE)
      len += write(fsc->bsfd, page+len, PAGE_SIZE-len);

  }
  free(page);
}

u_int32_t init_blockstore_hashpage(struct fs_ctx *fsc, u_int16_t sh, unsigned char lh[8], char *buf) {
  char *page = calloc(1, PAGE_SIZE);
  struct blockstore_hashpage_hdr *hdr = (struct blockstore_hashpage_hdr *)page;
  u_int32_t block_offset = fsc->next_block;
  struct blockstore_hashpage_entry *bshp = (struct blockstore_hashpage_entry *)page + sizeof(struct blockstore_hashpage_hdr);
  int len = 0;

  hdr->magic = BLOCKSTORE_MAGIC_HASHPAGE;
  hdr->sh = sh;
  hdr->entries = 1;
  bcopy(lh,&bshp->lh,8);
  fsc->next_block++;
  bshp->offset = fsc->next_block;

  lseek(fsc->bsfd, block_offset << 12, SEEK_SET);
  /*Write hashpage entry*/
  while(len<PAGE_SIZE)
    len += write(fsc->bsfd, page+len, PAGE_SIZE-len);

  free(page);

  len = 0;
  lseek(fsc->bsfd, fsc->next_block << 12, SEEK_SET);
  /*Write data buffer entry*/
  while(len<PAGE_SIZE)
    len += write(fsc->bsfd, buf+len, PAGE_SIZE-len);

  fsc->next_block++;
  return block_offset;
}

int update_blockstore_hashpage(struct fs_ctx *fsc, u_int16_t sh, unsigned char lh[8], char *buf) {
  char *page = malloc(PAGE_SIZE);
  struct blockstore_hashpage_hdr *hdr = (struct blockstore_hashpage_hdr *)page;
  struct blockstore_hashpage_entry *bshp;
  int i, len, ret=0;

  /*Read in the hashpage*/
  DPRINTF("Seek to hashpage: %d\n",fsc->bslt[sh]);
  lseek(fsc->bsfd, fsc->bslt[sh], SEEK_SET);
  if(read(fsc->bsfd, page, PAGE_SIZE)!=PAGE_SIZE) {
    ret = -1;
    goto done;
  }

  /*Check header*/
  if(hdr->magic != BLOCKSTORE_MAGIC_HASHPAGE) {
    DPRINTF("UPDATE_BLOCKSTORE: Bad page header, exiting...\n");
    ret = -1;
    goto done;
  }
  DPRINTF("Read hashpage header: %d,%d,%d\n",hdr->magic,hdr->sh,hdr->entries);

  for(i=0;i<hdr->entries;i++) {
    bshp = (struct blockstore_hashpage_entry *)page + (i * sizeof(struct blockstore_hashpage_entry)) + sizeof(struct blockstore_hashpage_hdr);
    if (bytecompare(lh,bshp->lh,8)) {
      DPRINTF("Found a match!\n");
      ret = 1;
      goto done;
    }
  }
  DPRINTF("No match\n");

  /*No existing match, so add a new entry*/
  if (hdr->entries++ > MAX_HASHPAGE_ENTRIES) {
    DPRINTF("Ran out of space on this HASHPAGE entry\n");
    ret = -1;
    goto done;
  }
  bshp = (struct blockstore_hashpage_entry *)page + ((hdr->entries-1)*sizeof(struct blockstore_hashpage_entry)) + sizeof(struct blockstore_hashpage_hdr);
  bcopy(lh,bshp->lh,8);
  bshp->offset = fsc->next_block;

  lseek(fsc->bsfd, fsc->next_block << 12, SEEK_SET);
  /*Write data buffer entry*/
  while(len<PAGE_SIZE)
    len += write(fsc->bsfd, buf+len, PAGE_SIZE-len);

  lseek(fsc->bsfd, fsc->bslt[sh], SEEK_SET);
  len = 0;
  /*Write hashpage entry*/
  while(len<PAGE_SIZE)
    len += write(fsc->bsfd, page+len, PAGE_SIZE-len);

  fsc->next_block++;
  ret = 1;

done:
  free(page);
  return ret;  
}

int retrieve_block(struct fs_ctx *fsc, u_int16_t sh, unsigned char lh[8], char *buf) {
  struct blockstore_hashpage_hdr *hdr = (struct blockstore_hashpage_hdr *)buf;
  struct blockstore_hashpage_entry *bshp;
  int i;

  /*Read in the hashpage*/
  lseek(fsc->bsfd, (u_int32_t)fsc->bslt[sh], SEEK_SET);
  if(read(fsc->bsfd, buf, PAGE_SIZE)!=PAGE_SIZE)
    return -1;

  /*Check header*/
  if(hdr->magic != BLOCKSTORE_MAGIC_HASHPAGE) {
    DPRINTF("RETRIEVE BLOCK: Bad page header, exiting...\n");
    return -1;
  }
  for(i=0;i<hdr->entries;i++) {
    bshp = (struct blockstore_hashpage_entry *)buf + (i * sizeof(struct blockstore_hashpage_entry)) + sizeof(struct blockstore_hashpage_hdr);
    if (bytecompare(lh,bshp->lh,8)) {
      lseek(fsc->bsfd, (bshp->offset<<12), SEEK_SET);
      if(read(fsc->bsfd, buf, PAGE_SIZE)!=PAGE_SIZE) {
	DPRINTF("Failed read\n");
	return -1;
      }
      return 1;
    }
  }
  return -1;
}

void update_bslt(struct fs_ctx *fsc, u_int32_t sh, u_int64_t offset) {
  int sector_offset, tmp;
  int len;
  char *p = (char *)fsc->bslt;

  fsc->bslt[sh] = offset*PAGE_SIZE;
  /*Find sector offset*/
  sector_offset = rounddown((&fsc->bslt[sh] - &fsc->bslt[0])*4,SECTOR_SIZE);
  lseek(fsc->bsfd, sector_offset + 4096, SEEK_SET);
  len = write(fsc->bsfd, p + sector_offset, SECTOR_SIZE);
}

int write_block(struct fs_ctx *fsc, char *buf, struct table_mask_entry *tme) {
  /*All buffer data must be 4k padded*/
  u_int16_t sh = gen_shorthash(buf);
  unsigned char *lh = gen_longhash(buf, &fsc->sha);
  u_int64_t offset;

  bcopy(lh, tme->lh, 8);
  tme->sh = sh;
  DPRINTF("SHash: %d\n",sh);
  DPRINTF("LHash: ");
  printhex(tme->lh,8);
  if(fsc->bslt[sh] == 0) {
    DPRINTF("Hashpage %d not allocated\n", sh);
    offset = init_blockstore_hashpage(fsc, sh, lh, buf);
    if (offset>0) update_bslt(fsc, (u_int32_t)sh, offset);
    else return -1;
  } else {
    DPRINTF("Hashpage %d allocated\n", sh);
    if(update_blockstore_hashpage(fsc, sh, lh, buf)!=1)
      return -1;
  }
  free(lh);
  return 1;
}

int read_block(struct fs_ctx *fsc, char *buf, u_int16_t sh, unsigned char *lh) {

  if(fsc->bslt[sh] == 0) {
    DPRINTF("Block not allocated in blockstore\n");
    return -1;
  }
  if (retrieve_block(fsc, sh, lh, buf)!=1) {
      return -1;
  }
  return 1;
}

