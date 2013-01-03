/* skeinwrap.c
 *
 * Wrappers for standard skein operations for the eKey
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include "skeinwrap.h"

static unsigned char keybuf[44]; /* 12 bytes serial, 32 bytes secret */

void 
PrepareSkein(EKeySkein *skein,
              const unsigned char *serial,
              const unsigned char *secret,
              const char *personalisation)
{
  int i;
  for (i = 0; i < 12; ++i) keybuf[i] = serial[i];
  for (i = 0; i < 32; ++i) keybuf[i + 12] = secret[i];
  
  Skein_256_InitExt(skein, 256, SKEIN_CFG_TREE_INFO_SEQUENTIAL,
                    keybuf, 44);
  Skein_Start_New_Type(skein, PERS);
  Skein_256_Update(skein, (unsigned char *)personalisation, 96);
  Skein_Start_New_Type(skein, MSG);
}
