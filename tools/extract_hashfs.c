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
  printf("Usage:\n\textract <Dir> <INPUT>\n");
}

int main(int argc, char *argv[]) {
  struct dirent* dirp;
  DIR* d;
  char *dir, *path;
  int outfd, infd;
  struct stat st;
  struct fs_hdr *fsh;
  int mseek = 0;
  int size, i;
  struct fs_ctx *fsc;

  if (argc != 4) {
    usage();
    return -1;
  }

  infd = open(argv[2],O_RDONLY);
  if (infd == -1) {
    printf("Failed to open input file\n");
    return -1;
  }

  dir = argv[1];
  if(chdir(dir)!=0) {
    printf("Failed to change dir to %s\n",dir);
    return -1;
  }

  fsc = init_fs_ctx();
  printf("Initialising blockstore...\n");
  if (init_blockstore(argv[3],fsc)!=1) {
          printf("Failed to initialise blockstore\n");
          return -1;
  }

  fsc->fsfd = infd;
  
  for(i=0; i<(START_OFFSET >> 9); i++) {
    mseek = i * SECTOR_SIZE;
    lseek(infd, mseek, SEEK_SET);
    fsh = read_hdr(infd, BLOCKSUM_MAGIC_HDR);
    if(!fsh) break;
    printf("Node: %s, size: %llu, offset %llu\n",
	   fsh->filename, fsh->length, fsh->offset);

    //Extract file to dir location
    outfd = open(fsh->filename, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);

    fcopy_from_blockstore(outfd,fsh->offset,fsc,fsh->length);
    ftruncate(outfd,fsh->length);

    free(fsh);
    close(outfd);
  }

  close(infd);
}
  
  

