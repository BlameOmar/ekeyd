/* frames/frame.c
 *
 * Framing information for eKey protocol
 *
 * Copyright 2009 Simtec Electronics
 *
 * For licence terms refer to the COPYING file.
 */

#include "frame.h"
#include "../skeinwrap.h"
#include "pem.h"

static EKeySkein framer_mac_base;
static EKeySkein framer_mac;

extern unsigned char *_serial_no;

extern void *memcpy(void*,const void*,size_t);

void
framer_prepare_mac(const unsigned char *sharedkey)
{
  PrepareSkein(&framer_mac_base, _serial_no, sharedkey, EKEY_SKEIN_PERSONALISATION_PMS);
}

void
framer_compute_mac(const char *frame, char *mactarget)
{
  unsigned char macbuf[32];
  memcpy(&framer_mac, &framer_mac_base, sizeof(framer_mac));
  Skein_256_Update(&framer_mac, frame, 52);
  Skein_256_Final_Pad(&framer_mac, macbuf);
  pem64_encode_bytes(macbuf, 3, mactarget);
  pem64_encode_bytes(macbuf + 29, 3, mactarget + 4);
}

void
framer_fill_frame(char *frame, char ft1, char ft2, char *payload)
{
  char *fr = frame + 2;
  int payload_left = 50;
  *fr++ = ft1;
  *fr++ = ft2;
  while ((payload_left > 0) && (*payload != 0)) {
    *fr++ = *payload++;
    payload_left--;
  }
  while (payload_left-- > 0)
    *fr++ = ' ';
  framer_compute_mac(frame + 2, fr);
}


#ifdef FRAMER_TEST

unsigned char *_serial_no = "\x00\x01\x02\x03\x10\x11\x12\x13\xab\xac\xad\xae";

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

static unsigned char nullkey[32];

int
main(int argc, char **argv)
{
  char message[64];
  char frame[64];
  int counter = 0;
  int max = 50;
  frame[0] = '*';
  frame[1] = ' ';
  frame[62] = '\r';
  frame[63] = '\n';
  
  memset(&(nullkey[0]), 0, 32);
  
  framer_prepare_mac(&(nullkey[0]));
  
  message[0] = message[1] = ' ';
  message[2 + pem64_encode_bytes(_serial_no, 12, &(message[2]))] = '\0';
  
  framer_fill_frame(frame, 'S', '!', message);
  write(1, frame, 64);
  
  if (argc > 1) {
    if (atoi(argv[1]) > 0)
      max = atoi(argv[1]);
  }
  
  for (counter = 0; counter < max; ++counter) {
    snprintf(message, 64, "Frame number %d from pid %d (parent %d)", counter, getpid(), getppid());
    framer_fill_frame(frame, 'I', '>', message);
    write(1, frame, 64);
  }
  
  return 0;
}

#endif
