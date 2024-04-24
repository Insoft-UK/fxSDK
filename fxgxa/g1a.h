//---
//  fxgxa:g1a - Add-in header for Casio's G1A format
//---

#ifndef FX_G1A
#define FX_G1A

#include <stdint.h>

/* TODO: eStrips are not supported yet */
struct g1a_estrip
{
    uint8_t data[80];
};

/* G1A file header with 0x200 bytes. When output to a file the standard part
   (first 0x20 bytes) of this header is bit-inverted, but the non-inverted
   version makes a lot more sens so we'll be using it. */
struct g1a_header
{                               /* Offset Size  Value                       */
    char     magic[8];          /* 0x000  8     "USBPower"                  */
    uint8_t  mcs_type;          /* 0x008  1     0xf3 (AddIn)                */
    uint8_t  seq1[5];           /* 0x009  5     0x0010001000                */
    uint8_t  control1;          /* 0x00e  1     *0x13 + 0x41                */
    uint8_t  seq2;              /* 0x00f  1     0x01                        */
    uint32_t filesize_be1;      /* 0x010  4     File size, big endian       */
    uint8_t  control2;          /* 0x014  1     *0x13 + 0xb8                */
    uint8_t  _1;                /* 0x015  1     ???                         */
    uint16_t checksum;          /* 0x016  2     BE sum of 8 shorts at 0x300 */
    uint8_t  _2[6];             /* 0x018  6     ???                         */
    uint16_t mcs_objects;       /* 0x01e  2     MCS-only, unused            */

    char     internal[8];       /* 0x020  8     Internal app name with '@'  */
    uint8_t  _3[3];             /* 0x028  3     ???                         */
    uint8_t  estrips;           /* 0x02b  1     Number of estrips (0..4)    */
    uint8_t  _4[4];             /* 0x02c  4     ???                         */
    char     version[10];       /* 0x030  10    Version "MM.mm.pppp"        */
    uint8_t  _5[2];             /* 0x03a  2     ???                         */
    char     date[14];          /* 0x03c  14    Build date "yyyy.MMdd.hhmm" */
    uint8_t  _6[2];             /* 0x04a  2     ???                         */
    uint8_t  icon[68];          /* 0x04c  68    30*17 monochrome icon       */
    struct g1a_estrip estrip1;  /* 0x090  80    eStrip 1                    */
    struct g1a_estrip estrip2;  /* 0x0e0  80    eStrip 2                    */
    struct g1a_estrip estrip3;  /* 0x130  80    eStrip 3                    */
    struct g1a_estrip estrip4;  /* 0x180  80    eStrip 4                    */
    uint8_t  _7[4];             /* 0x1d0  4     ???                         */
    char     name[8];           /* 0x1d4  8     Add-in name                 */
    uint8_t  _8[20];            /* 0x1dc  20    ???                         */
    uint32_t filesize_be2;      /* 0x1f0  4     File size, big endian       */
    uint8_t  _9[12];            /* 0x1f4  12    ???                         */
};

/* A full g1a file, suitable for use with pointers */
struct g1a
{
    struct g1a_header header;
    uint8_t code[];
};

#endif /* FX_G1A */
