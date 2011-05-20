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

struct fs_hdr *init_hdr(char *filename, int length, u_long offset, u_short magic) {
  struct fs_hdr *fsh;
  fsh = malloc(sizeof(struct fs_hdr));
  fsh->magic = magic;
  fsh->offset = offset;
  fsh->length = length;
  strncpy(fsh->filename, filename, 485);
  return fsh;
}

struct fs_hdr *read_hdr(int fd, u_short magic) {
  struct fs_hdr *fsh;

  fsh = malloc(sizeof(struct fs_hdr));

  //only handle sector reads
  if (read(fd, fsh, SECTOR_SIZE)!=SECTOR_SIZE)
    goto exit;
  if(fsh->magic != magic)
    goto exit;

  return fsh;
 exit:
  free(fsh);
  return NULL;
}

int fcopy(int infd, int outfd, u_long length) {
  char buf[SECTOR_SIZE], *p;
  int inbytes, outbytes, count=0;

  while(length > 0) {
    bzero(buf,SECTOR_SIZE);
    inbytes = read(infd, buf, SECTOR_SIZE);
    if(inbytes < 1)
      return count;
    
    length -= SECTOR_SIZE;
    outbytes = write(outfd, buf, SECTOR_SIZE);
    if(outbytes != SECTOR_SIZE)
	return count;
    count += outbytes;
  }
  return count;  
}

struct fs_lookup_table *init_fstable_entry(char *page) {
  struct fs_lookup_table *fslt = (struct fs_lookup_table *)page;
  bzero(page,PAGE_SIZE);
  fslt->magic = FSLOOKUP_TABLE_HDR;
  return;
}

int flush_fslt(struct fs_ctx *fsc, char *page) {
  /*Write the FSlookup table*/
  lseek(fsc->fsfd, fsc->next_fs_block << 12, SEEK_SET);
  int len = 0;
  while(len<PAGE_SIZE)
    len += write(fsc->fsfd, page + len, PAGE_SIZE-len);
  fsc->next_fs_block++;
}

u_int32_t fcopy_to_blockstore(int infd, struct fs_ctx *fsc, u_int32_t length) {
  char *buf = malloc(PAGE_SIZE);
  char *p, *page = calloc(1,PAGE_SIZE);
  int inbytes, outbytes;
  int count = 0, mdpagecount;
  struct table_mask_entry *tme;
  struct fs_lookup_table *fslt;
  u_int32_t offset = fsc->next_fs_block;
  u_int32_t pages = length >> 12;
  if(length % PAGE_SIZE)
    pages += 1;
  
  fslt = init_fstable_entry(page);
  mdpagecount = MAX_FSTABLE_ENTRIES;
  while(pages > 0) {
    bzero(buf,PAGE_SIZE);
    inbytes = read(infd, buf, PAGE_SIZE);
    if(inbytes < 1) {
      free(page);
      free(buf);    
      return count;
    }
    
    pages--;
    tme = (struct table_mask_entry *)page + sizeof(struct fs_lookup_table) + (count * sizeof(struct table_mask_entry));
    write_block(fsc, buf, tme);
    count++;
    mdpagecount--;

    if(mdpagecount == 0) {
      fslt->next_offset = fsc->next_fs_block+1;
      flush_fslt(fsc,page);
      count = 0;
      mdpagecount = MAX_FSTABLE_ENTRIES;
      init_fstable_entry(page);
    }
  }

  flush_fslt(fsc,page);
  free(page);
  free(buf);
  return offset;  
}

int get_fslt_entry(struct fs_ctx *fsc, char *page, u_int32_t offset) {
  struct fs_lookup_table *fslt = (struct fs_lookup_table *)page;

  lseek(fsc->fsfd, offset<<12,SEEK_SET);
  if (read(fsc->fsfd, page, PAGE_SIZE)!=PAGE_SIZE) {
    DPRINTF("Failed to read all data\n");
    return -1;
  }

  if(fslt->magic != FSLOOKUP_TABLE_HDR) {
    DPRINTF("Bad FSLT header!\n");
    return -1;
  }
  return 1;
}

int fcopy_from_blockstore(int outfd, u_int32_t offset, struct fs_ctx *fsc, u_int32_t length) {
  int count = 0, mdpagecount;
  struct table_mask_entry *tme;
  char *fsltpage = malloc(PAGE_SIZE);
  struct fs_lookup_table *fslt = (struct fs_lookup_table *)fsltpage;
  char *p = malloc(PAGE_SIZE);
  u_int32_t pages = length >> 12;
  if((pages*PAGE_SIZE)!=length)
    pages += 1;

  if(get_fslt_entry(fsc, fsltpage, offset)!=1)
    return -1;

  mdpagecount = MAX_FSTABLE_ENTRIES;
  while(pages > 0) {
    tme = (struct table_mask_entry *)fsltpage + sizeof(struct fs_lookup_table) + (count * sizeof(struct table_mask_entry));
    if(tme->sh!=0) {
      if(read_block(fsc, p, tme->sh, tme->lh)==1)
	write(outfd, p, PAGE_SIZE);
    }
    count++;
    mdpagecount--;
    pages--;

    if(mdpagecount == 0) {
      if(get_fslt_entry(fsc, fsltpage, fslt->next_offset)!=1)
	return -1;
      count = 0;
      mdpagecount = MAX_FSTABLE_ENTRIES;
    }
  }
  free(p);
  return 1;
}

int fmetadata_init(int outfd, u_int32_t offset, struct fs_ctx *fsc, u_int32_t length) {
  int count = 0, j,lookup_offset=0;
  struct table_mask_entry *tme;
  char *fsltpage = malloc(PAGE_SIZE);
  struct fs_lookup_table *fslt = (struct fs_lookup_table *)fsltpage;

  u_int32_t pages = length >> 12;
  if((pages*PAGE_SIZE)!=length)
    pages += 1;
  char *lookup = calloc(1,sizeof(struct table_mask_entry)*pages);
  DPRINTF("FMETADATA: allocated %d bytes (%d pages) for lookup table\n",sizeof(struct table_mask_entry)*pages,pages);

  if(get_fslt_entry(fsc, fsltpage, offset)!=1)
    return -1;

  pages = 0;
  for (j=0; j<pages;j++) {
    tme = (struct table_mask_entry *)fsltpage + sizeof(struct fs_lookup_table) + (count * sizeof(struct table_mask_entry));
    //bcopy(tme, lookup + sizeof(struct table_mask_entry)*lookup_offset,sizeof(struct table_mask_entry));
    DPRINTF("\tTME VAL: %d,%d\n",lookup_offset,tme->sh);
    count++;
    lookup_offset++;

    if(count == MAX_FSTABLE_ENTRIES) {
      if(get_fslt_entry(fsc, fsltpage, fslt->next_offset)!=1)
	return -1;
      count = 0;
    }
  }
  fsc->FSMDlookup = lookup;
  free(fsltpage);
  return 1;
}

int fzero(int outfd, int length) {
  //Length must be a multiple of page size
  char *buf;
  int i;

  buf = calloc(1,PAGE_SIZE);
  for(i=0; i<(length/PAGE_SIZE); i++)
    write(outfd, buf, PAGE_SIZE);
  free(buf);
  return 1;
}
