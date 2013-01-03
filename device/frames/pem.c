/* frames/pem.c
 *
 * PEM64 encoding for framing.
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include "pem.h"

#ifdef TEST_PEM

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#endif

static const char *dictionary =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int inverse_dictionary[128] =
  { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /*  0 -- 9  */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* 10 -- 19 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* 20 -- 29 */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  /* 30 -- 39 */
    -1, -1, -1, 62, -1, -1, -1, 63, 52, 53,  /* 40 -- 49 */
    54, 55, 56, 57, 58, 59, 60, 61, -1, -1,  /* 50 -- 59 */
    -1,  0, -1, -1, -1,  0,  1,  2,  3,  4,  /* 60 -- 69 */
     5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  /* 70 -- 79 */
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24,  /* 80 -- 89 */
    25, -1, -1, -1, -1, -1, -1, 26, 27, 28,  /* 90 -- 99 */
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38,  /* 100--109 */
    39, 40, 41, 42, 43, 44, 45, 46, 47, 48,  /* 110--119 */
    49, 50, 51, -1, -1, -1, -1, -1 };        /* 120--127 */

#define FILLERCHAR ('=')

/* exported interface documented in pem.h */
int 
pem64_encode_bytes_length(int nbytes)
{
    return ((nbytes + 2) / 3) * 4;
}

int pem64_encode_bytes(const unsigned char *inbytes,
                       int nbytes,
                       char *outtext)
{
  int filledcount = 0;
  while (nbytes >= 3) {
    int 
      c1 = inbytes[0], 
      c2 = inbytes[1],
      c3 = inbytes[2];
    
    *(outtext++) = dictionary[c1 >> 2];
    *(outtext++) = dictionary[((c1 & 0x3) << 4) | (c2 >> 4)];
    *(outtext++) = dictionary[((c2 & 0xf) << 2) | (c3 >> 6)];
    *(outtext++) = dictionary[c3 & 0x3f];
    
    filledcount += 4;
    nbytes -= 3;
    inbytes += 3;
  }
  if (nbytes == 2) {
    int 
      c1 = inbytes[0], 
      c2 = inbytes[1];
    
    *(outtext++) = dictionary[c1 >> 2];
    *(outtext++) = dictionary[((c1 & 0x3) << 4) | (c2 >> 4)];
    *(outtext++) = dictionary[((c2 & 0xf) << 2)];
    *(outtext++) = FILLERCHAR;
    filledcount += 4;
  } else if (nbytes == 1) {
    int 
      c1 = inbytes[0];
    
    *(outtext++) = dictionary[c1 >> 2];
    *(outtext++) = dictionary[((c1 & 0x3) << 4)];
    *(outtext++) = FILLERCHAR;
    *(outtext++) = FILLERCHAR;
    filledcount += 4;
  }
  
  return filledcount;
}

int pem64_decode_bytes(const char *intext,
                       int nchars,
                       unsigned char *outbytes)
{
  unsigned char *outbcopy = outbytes;
  while (nchars >= 4) {
    int
      c1 = intext[0], 
      c2 = intext[1],
      c3 = intext[2],
      c4 = intext[3];
    int 
      b1 = inverse_dictionary[c1],
      b2 = inverse_dictionary[c2],
      b3 = inverse_dictionary[c3],
      b4 = inverse_dictionary[c4];

    if ((b1 == -1) || (b2 == -1) || (b3 == -1) || (b4 == -1))
      return outbytes - outbcopy;

    *(outbytes++) = (b1 << 2) | (b2 >> 4);

    if (c3 != FILLERCHAR)
      *(outbytes++) = ((b2 & 0xf) << 4) | (b3 >> 2);
    if (c4 != FILLERCHAR)
      *(outbytes++) = ((b3 & 0x3) << 6) | b4;
    
    nchars -= 4;
    intext += 4;
  }
  
  return outbytes - outbcopy;
}

void
pem64_encode_12bits(const short value, char *outtext)
{
  *(outtext++) = dictionary[value & 0x3f];
  *(outtext++) = dictionary[(value >> 6) & 0x3f];
}

short
pem64_decode_12bits(const char *intext)
{
  int
    b1 = inverse_dictionary[(int) intext[0]],
    b2 = inverse_dictionary[(int) intext[1]];
  
  if ((b1 == -1) || (b2 == -1))
    return 0;
  
  return (b1 | (b2 << 6));
  
}

#ifdef TEST_PEM

static char pem_buffer[4096];

static void
run_single_test(const unsigned char *buffer,
                const int buflen)
{
  unsigned char *bcopy = calloc(buflen, 1);
  int pemchars = pem64_encode_bytes(buffer, buflen, pem_buffer);
  int pembytes = pem64_decode_bytes(pem_buffer, pemchars, bcopy);
  
  assert((pemchars % 4) == 0);
  assert(pembytes == buflen);
  assert(memcmp(bcopy, buffer, pembytes) == 0);
    
  free(bcopy);
}

#define R(S) run_single_test(S, sizeof(S) - 1)

int
main(int argc, char **argv)
{
  R("123");
  R("\1\2\3");
  R("\1\2");
  R("1234");
  R("12345");
  R("123456");
  
  return 0;
}

#endif
