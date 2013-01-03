/* skeinwrap.h
 *
 * Wrappers for standard skein operations for the eKey
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#ifndef EKEY_SKEINWRAP_H
#define EKEY_SKEINWRAP_H

#include "skein/skein.h"

typedef Skein_256_Ctxt_t EKeySkein;

extern void PrepareSkein(EKeySkein *skein,
                         const unsigned char *serial,
                         const unsigned char *secret,
                         const char *personalisation);

/* 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456789 123456 */  
#define EKEY_SKEIN_PERSONALISATION_LRS                                  \
  "20090609 support@simtec.co.uk EntropyKey/v1/LongTermReKeyingState                               "
#define EKEY_SKEIN_PERSONALISATION_RS                                   \
  "20090609 support@simtec.co.uk EntropyKey/v1/ReKeyingState                                       "
#define EKEY_SKEIN_PERSONALISATION_PMS                                  \
  "20090609 support@simtec.co.uk EntropyKey/v1/MessageAuthenticationCodeState                      "
#define EKEY_SKEIN_PERSONALISATION_EES                                  \
  "20090609 support@simtec.co.uk EntropyKey/v1/EntropyEncryptionState                              "
#define EKEY_SKEIN_PERSONALISATION_LKMS                                 \
  "20090609 support@simtec.co.uk EntropyKey/v1/MessageAuthenticationCodeStateForLongTermReKeying   "


#endif /* EKEY_SKEINWRAP_H */
