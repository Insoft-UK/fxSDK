//---
//  fxgxa:g3a - Add-in header for Casio's G3A format
//---

#ifndef FX_G3A
#define FX_G3A

#include <stdint.h>

/* G3A file header with 0x7000 bytes. When output to a file the standard part
   (first 0x20 bytes) of this header is bit-inverted. */
struct g3a_header
{                                  /* Offset|Size|Value                      */
    char     magic[8];             /* 0x000  8    "USBPower"                 */
    uint8_t  mcs_type;             /* 0x008  1    0x2c (AddIn)               */
    uint8_t  seq1[5];              /* 0x009  5    0x0001000100               */
    uint8_t  control1;             /* 0x00e  1    *0x13 + 0x41               */
    uint8_t  seq2;                 /* 0x00f  1    0x01                       */
    uint32_t filesize_be1;         /* 0x010  4    File size, big endian      */
    uint8_t  control2;             /* 0x014  1    *0x13 + 0xb8               */
    uint8_t  _1;                   /* 0x015  1    ???                        */
    uint16_t checksum;             /* 0x016  2    BE sum of 8 shorts at 7100 */
    uint8_t  _2[6];                /* 0x018  6    ???                        */
    uint16_t mcs_objects;          /* 0x01e  2    MCS-only, unused           */

    uint32_t checksum_2;           /* 0x020  4    Checksum: 0..1f+24..EOF-4  */
    uint8_t  seq3[2];              /* 0x024  2    0x0101                     */
    uint8_t  _3[8];                /* 0x026  8    ???                        */
    uint32_t filesize_be2;         /* 0x02e  4    Filesize - 0x7000 - 4      */
    uint8_t  _4[14];               /* 0x032  14   ???                        */
    char     name[16];             /* 0x040  16   Add-in name + NUL          */
    uint8_t  _5[12];               /* 0x050  12   ???                        */
    uint32_t filesize_be3;         /* 0x05c  4    Filesize                   */
    char     internal[11];         /* 0x060  11   Internal name with '@'     */
    char     label[8][24];         /* 0x06b  192  Language labels            */
    uint8_t  allow_estrip;         /* 0x12b  1    Allow use as eAct strip?   */
    uint8_t  _6[4];                /* 0x12c  4    ???                        */
    char     version[10];          /* 0x130  10   Version "MM.mm.pppp"       */
    uint8_t  _7[2];                /* 0x13a  2    ???                        */
    char     date[14];             /* 0x13c  14   Date "yyyy.MMdd.hhmm"      */
    uint8_t  _8[38];               /* 0x14a  38   ???                        */
    char     estrip_label[8][36];  /* 0x170  288  eStrip language labels     */
    uint8_t  eact_icon[0x300];     /* 0x290  768  eAct icon (64x24)          */
    uint8_t  _9[0x92c];            /* 0x590  2348 ???                        */
    char     filename[324];        /* 0xebc  324  Filename "X.g3a"           */
    uint16_t icon_uns[0x1800];     /* 0x1000 5376 Unselected icon            */
    uint16_t icon_sel[0x1800];     /* 0x4000 5376 Selected icon              */

} __attribute__((packed, aligned(4)));

/* A full g3a file, suitable for use with pointers */
struct g3a
{
    struct g3a_header header;
    uint8_t code[];
};

#endif /* FX_G3A */
