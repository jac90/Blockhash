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

void usage() {
  printf("Usage:\n\tcreate_hashnode <Filename> <Size in MB>\n");
}

int main(int argc, char *argv[]) {
  int outfd;
  struct fs_hdr *fsh;
  int size, entries, i, offset;
  struct fs_ctx *fsc;
  struct fs_lookup_table *fslt;
  char *page = calloc(1,4096);

  if (argc != 3) {
    usage();
    return -1;
  } 

  outfd = open(argv[1],O_WRONLY);
  if (outfd == -1 && errno == ENOENT) {
    outfd = open(argv[1],O_CREAT|O_WRONLY);
  }
  if (outfd == -1) {
    printf("Failed to open output file\n");
    return -1;
  }
  printf("Opened output file %s\n",argv[1]);

  size = atoi(argv[2]);
  printf("Requested %d bytes (%s Mbytes)\n",size<<20,argv[2]);

  //Initialise FS Context
  fsc = init_fs_ctx();
  fsc->next_fs_block = 1;
  
  fsc->fsfd = outfd;

  //fsh = init_hdr("BLKTAP", size, fsc->next_fs_block, BLOCKSUM_MAGIC_HDR);
  //lseek(outfd,0,SEEK_SET);
  //write(outfd, fsh, sizeof(struct fs_hdr));

  entries = (size*256)/MAX_FSTABLE_ENTRIES;
  if((size*256)/MAX_FSTABLE_ENTRIES!=entries) 
	  entries++;
  printf("Calculated %d page table entries\n",size*256);
  printf("FSlookup table entries: %d\n",entries);
  
  //Now write the FS lookup page table entries
  fslt = (struct fs_lookup_table *)page;
  fslt->magic = FSLOOKUP_TABLE_HDR;
  for (i=0; i<entries; i++) {
          offset = (i+1) << 12;
          //printf("Creating entry %d at offset %d\n",i,offset);
          if (i < entries-1)
                  fslt->next_offset = i + 2;
          lseek(outfd, offset, SEEK_SET);
          write(outfd, page, PAGE_SIZE);
  }

  fsh = init_hdr("BLKTAP", size<<20, fsc->next_fs_block, BLOCKSUM_MAGIC_HDR);
  lseek(outfd,0,SEEK_SET);
  write(outfd, fsh, sizeof(struct fs_hdr));
  free(fsh);
  free(page);
  close(outfd);
  return 0;
}
