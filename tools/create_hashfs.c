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
  printf("Usage:\n\tcreate <Dir> <OUTPUTFSNode> <OUTPUTBlockstoreDev>\n");
}

int main(int argc, char *argv[]) {
  struct dirent* dirp;
  DIR* d;
  char *dir, *path;
  int outfd, infd;
  struct stat st;
  struct fs_hdr *fsh;
  int mseek = 0;
  int size;
  u_int32_t offset;
  struct fs_ctx *fsc;

  if (argc != 4) {
    usage();
    return -1;
  } 

  outfd = open(argv[2],O_WRONLY);
  if (outfd == -1 && errno == ENOENT) {
    outfd = open(argv[2],O_CREAT|O_WRONLY);
  }
  if (outfd == -1) {
    printf("Failed to open output file\n");
    return -1;
  }
  printf("Opened output file %s\n",argv[2]);

  //Zero out metadata region and reset filehandle
  fzero(outfd, START_OFFSET);
  lseek(outfd,mseek,SEEK_SET);

  //Initialise the blockstore
  fsc = init_fs_ctx();
  printf("Initialising blockstore...\n");
  if (init_blockstore(argv[3],fsc)!=1) {
	  printf("Failed to initialise blockstore\n");
	  return -1;
  }
  fsc->next_fs_block = 1024;
  
  fsc->fsfd = outfd;

  dir = strdup(argv[1]);
  if ((d = opendir(argv[1])) == NULL) {
    return -1;
  }

  while ((dirp = readdir(d)) != NULL) {
    if (dirp->d_type == DT_REG) {
      printf("%s/%s\n", dir, dirp->d_name);
      size = strlen(dir) + strlen(dirp->d_name) + 2;
      path = malloc(size);
      if(!path) {
	printf("Failed to malloc %d bytes\n",size);
	continue;
      }
      snprintf(path,size,"%s/%s", dir, dirp->d_name);

      //Open the file and write to dst
      infd = open(path, O_RDONLY);
      free(path);

      if(fstat(infd, &st)!=0) {
	printf("Failed to stat input file, continuing...\n");
	continue;
      }

      offset = fcopy_to_blockstore(infd,fsc,roundup(st.st_size,PAGE_SIZE));
      close(infd);

      //Seek to location and Write FS metadata
      fsh = init_hdr(dirp->d_name, st.st_size, offset, BLOCKSUM_MAGIC_HDR);
      lseek(outfd,mseek,SEEK_SET);
      write(outfd, fsh, sizeof(struct fs_hdr));

      printf("Wrote Node: %s, size: %llu, offset: %llu\n",
           fsh->filename, fsh->length, fsh->offset);

      //Reset FD pointers
      mseek += SECTOR_SIZE;

      free(fsh);
      close(infd);
    }
  }
  close(outfd);
  closedir(d);
  close_blockstore(fsc);
  return 0;
}
