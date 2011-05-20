/*
 * CRC 32 Source: http://www.di-mgt.com.au/crypto.html#CRC
 */
#include "hashfs.h"

u_int16_t gen_shorthash(char *octets)
{
  unsigned long crc = 0xFFFFFFFF;
  unsigned long temp;
  int j, len = PAGE_SIZE;

  while (len--) 
  {
    temp = (unsigned long)((crc & 0xFF) ^ *octets++);
    for (j = 0; j < 8; j++) 
    {
      if (temp & 0x1)
        temp = (temp >> 1) ^ 0xEDB88320;
      else
        temp >>= 1;
    }
    crc = (crc >> 8) ^ temp;
  }
  return (u_int16_t)crc ^ 0xFFFFFFFF;
}

unsigned char *gen_longhash(char *octets, SHA_CTX *sha) {
  unsigned char *digest = malloc(20);
  SHAInit(sha);
  SHAUpdate(sha, octets, PAGE_SIZE);
  SHAFinal(digest, sha);
  return digest;
}

