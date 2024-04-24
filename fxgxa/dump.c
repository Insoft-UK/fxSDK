#include <stdio.h>
#include <string.h>
#include <endianness.h>

#include <fxgxa.h>
#include <g1a.h>

/* check_g1a(): Check validity of a g1a control or fixed field

   This function checks a single field of a g1a header (depending on the value
   of @test, from 0 up) and returns:
   * 0 if the field is valid
   * 1 if there is a minor error (wrong fixed-byte entry)
   * 2 if there is a major error (like not a g1a, bad checksum, etc)
   * -1 if the value of @test is out of bounds

   It produces a description of the check in @status (even if the test is
   passed); the string should have room for at least 81 bytes.

   @test    Test number
   @g1a     G1A file being manipulated
   @size    File size
   @status  Array row, at least 81 bytes free */
#define m(msg, ...) sprintf(status, msg, ##__VA_ARGS__)
static int check_g1a(int test, struct g1a const *g1a, size_t size,char *status)
{

	struct g1a_header const *h = &g1a->header;
	uint8_t const *raw = (void *)h;

	uint16_t sum;
	uint8_t ctrl;

	switch(test)
	{
	case 0:
		m("Signature    \"USBPower\"    \"########\"");
		strncpy(status + 28, h->magic, 8);
		return strncmp(h->magic, "USBPower", 8) ? 2:0;
	case 1:
		m("MCS Type     0xf3          0x%02x", h->mcs_type);
		return (h->mcs_type != 0xf3) ? 2:0;
	case 2:
		m("Sequence 1   0x0010001000  0x%02x%02x%02x%02x%02x",
		h->seq1[0], h->seq1[1], h->seq1[2], h->seq1[3], h->seq1[4]);
		return memcmp((const char *)h->seq1, "\x00\x10\x00\x10\x00",
			5) ? 1:0;
	case 3:
		ctrl = raw[0x13] + 0x41;
		m("Control 1    0x%02x          0x%02x", ctrl, h->control1);
		return (h->control1 != ctrl) ? 2:0;
	case 4:
		m("Sequence 2   0x01          0x%02x", h->seq2);
		return (h->seq2 != 0x01) ? 1:0;
	case 5:
		m("File size 1  %-8zu      %u", size,
			be32toh(h->filesize_be1));
		return (be32toh(h->filesize_be1) != size) ? 2:0;
	case 6:
		ctrl = raw[0x13] + 0xb8;
		m("Control 2    0x%02x          0x%02x", ctrl, h->control2);
		return (h->control2 != ctrl) ? 2:0;
	case 7:
		sum = checksum_g1a(g1a, size);
		m("Checksum     0x%02x        0x%02x", sum,
			be16toh(h->checksum));
		return (be16toh(h->checksum) != sum) ? 2:0;
	case 8:
		m("File size 2  %-8zu      %u", size,
			be32toh(h->filesize_be2));
		return (be32toh(h->filesize_be2) != size) ? 2:0;
	default:
		return -1;
	}
}

/* unknown(): Dump the contents of an unknown field */
static void unknown(uint8_t const *data, size_t offset, size_t size)
{
	printf("  0x%03zx   %-4zd  0x", offset, size);
	for(size_t i = 0; i < size; i++) printf("%02x", data[offset + i]);
	printf("\n");
}

/* field(): Print a potentially not NUL-terminated text field */
static void field(const char *field, size_t size)
{
	for(size_t i = 0; i < size && field[i]; i++) putchar(field[i]);
	printf("\n");
}

void dump_g1a(struct g1a const *g1a, size_t size)
{
	struct g1a_header const *header = &g1a->header;
	uint8_t const *raw = (void *)header;

	/* Checks for g1a files */
	char status[81];
	int ret=0, passed=0;

	printf("G1A signature checks:\n\n");
	printf("  Sta.  Field        Expected      Value\n");

	for(int test = 0; ret >= 0; test++)
	{
		ret = check_g1a(test, g1a, size, status);
		passed += !ret;
		if(ret < 0) break;

		printf("  %s  %s\n", ret ? "FAIL" : "OK  ", status);
	}

	printf("\nFields with unknown meanings:\n\n");
	printf("  Offset  Size  Value\n");

	unknown(raw, 0x015, 1);
	unknown(raw, 0x018, 6);
	unknown(raw, 0x028, 3);
	unknown(raw, 0x02c, 4);
	unknown(raw, 0x03a, 2);
	unknown(raw, 0x04a, 2);
	unknown(raw, 0x1d0, 4);
	unknown(raw, 0x1dc, 20);
	unknown(raw, 0x1f4, 12);

	printf("\nApplication metadata:\n\n");

	printf("  Program name:   ");
	field(header->name, 8);
	printf("  Internal name:  ");
	field(header->internal, 8);
	printf("  Version:        ");
	field(header->version, 10);
	printf("  Build date:     ");
	field(header->date, 14);

	printf("\nProgram icon:\n\n");
	icon_print_1(header->icon, 30, 17);
}

/* See check_g3a() for a description */
static int check_g3a(int test, struct g3a const *g3a, size_t size,char *status)
{
	struct g3a_header const *h = &g3a->header;
	uint8_t const *raw = (void *)h;

	uint16_t sum;
	uint32_t sum2;
	uint8_t ctrl;

	switch(test)
	{
	case 0:
		m("Signature    \"USBPower\"    \"########\"");
		strncpy(status + 28, h->magic, 8);
		return strncmp(h->magic, "USBPower", 8) ? 2:0;
	case 1:
		m("MCS Type     0x2c          0x%02x", h->mcs_type);
		return (h->mcs_type != 0x2c) ? 2:0;
	case 2:
		m("Sequence 1   0x0010001000  0x%02x%02x%02x%02x%02x",
		h->seq1[0], h->seq1[1], h->seq1[2], h->seq1[3], h->seq1[4]);
		return memcmp((const char *)h->seq1, "\x00\x01\x00\x01\x00",
			5) ? 1:0;
	case 3:
		ctrl = raw[0x13] + 0x41;
		m("Control 1    0x%02x          0x%02x", ctrl, h->control1);
		return (h->control1 != ctrl) ? 2:0;
	case 4:
		m("Sequence 2   0x01          0x%02x", h->seq2);
		return (h->seq2 != 0x01) ? 1:0;
	case 5:
		m("File size 1  %-8zu      %u", size,
			be32toh(h->filesize_be1));
		return (be32toh(h->filesize_be1) != size) ? 2:0;
	case 6:
		ctrl = raw[0x13] + 0xb8;
		m("Control 2    0x%02x          0x%02x", ctrl, h->control2);
		return (h->control2 != ctrl) ? 2:0;
	case 7:
		sum = checksum_g3a(g3a, size);
		m("Checksum     0x%02x        0x%02x", sum,
			be16toh(h->checksum));
		return (be16toh(h->checksum) != sum) ? 2:0;
	case 8:
		m("File size 2  %-8zu      %u", size - 0x7004,
			be32toh(h->filesize_be2));
		return (be32toh(h->filesize_be2) != size - 0x7004) ? 2:0;
	case 9:
		sum2 = checksum_g3a_2(g3a, size);
		m("Checksum 2   0x%08x    0x%08x", sum2,
			be32toh(h->checksum_2));
		return (be32toh(h->checksum_2) != sum2) ? 2:0;
	case 10:
		m("File size 1  %-8zu      %u", size,
			be32toh(h->filesize_be3));
		return (be32toh(h->filesize_be3) != size) ? 2:0;
	case 11:
		sum2 = checksum_g3a_2(g3a, size);
		uint32_t footer = be32toh(*(uint32_t *)((void *)g3a+size-4));
		m("Footer       0x%08x    0x%08x", sum2, footer);
		return (footer != sum2) ? 2:0;
	default:
		return -1;
	}
}

void dump_g3a(struct g3a const *g3a, size_t size)
{
	struct g3a_header const *header = &g3a->header;
	uint8_t const *raw = (void *)header;

	/* Checks for g3a files */
	char status[81];
	int ret=0, passed=0;

	printf("G3A signature checks:\n\n");
	printf("  Sta.  Field        Expected      Value\n");

	for(int test = 0; ret >= 0; test++)
	{
		ret = check_g3a(test, g3a, size, status);
		passed += !ret;
		if(ret < 0) break;

		printf("  %s  %s\n", ret ? "FAIL" : "OK  ", status);
	}

	printf("\nFields with unknown meanings:\n\n");
	printf("  Offset  Size  Value\n");

	unknown(raw, 0x015, 1);
	unknown(raw, 0x018, 6);
	unknown(raw, 0x026, 8);
	unknown(raw, 0x032, 14);
	unknown(raw, 0x050, 12);
	unknown(raw, 0x12c, 4);
	unknown(raw, 0x13a, 2);
	unknown(raw, 0x14a, 38);

	printf("  0x590   2348  ");
	bool is_zeros = true;
	for(int i = 0; i < 2348; i++)
		if(raw[0x590+i] != 0) is_zeros =1;
	printf(is_zeros ? "<All zeros>\n" : "<Not shown (non-zero)>\n");

	printf("\nApplication metadata:\n\n");

	printf("  Program name:   ");
	field(header->name, 16);
	printf("  Internal name:  ");
	field(header->internal, 11);
	printf("  Version:        ");
	field(header->version, 10);
	printf("  Build date:     ");
	field(header->date, 14);
	printf("  Filename:       ");
	field(header->filename, 324);

	printf("\nUnselected program icon:\n\n");
	icon_print_16(header->icon_uns, 92, 64);

	printf("\nSelected program icon:\n\n");
	icon_print_16(header->icon_sel, 92, 64);

	printf("\n");
}

void dump(void *gxa, size_t size)
{
	if(is_g1a(gxa))
		return dump_g1a(gxa, size);
	if(is_g3a(gxa))
		return dump_g3a(gxa, size);
}
