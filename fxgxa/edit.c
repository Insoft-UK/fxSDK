#include <fxgxa.h>
#include <stdio.h>
#include <string.h>
#include <endianness.h>

void sign(void *gxa, size_t size)
{
	if(is_g1a(gxa)) {
		struct g1a_header *header = gxa;

		/* Fixed elements */
		memcpy(header->magic, "USBPower", 8);
		header->mcs_type = 0xf3;
		memcpy(header->seq1, "\x00\x10\x00\x10\x00", 5);
		header->seq2 = 0x01;
		header->filesize_be1 = htobe32(size);
		header->filesize_be2 = htobe32(size);

		/* Control bytes and checksums */
		header->control1 = size + 0x41;
		header->control2 = size + 0xb8;
		header->checksum = htobe16(checksum_g1a(gxa, size));
	}
	else if(is_g3a(gxa)) {
		struct g3a_header *header = gxa;

		/* Fixed elements */
		memcpy(header->magic, "USBPower", 8);
		header->mcs_type = 0x2c;
		memcpy(header->seq1, "\x00\x01\x00\x01\x00", 5);
		header->seq2 = 0x01;
		memcpy(header->seq3, "\x01\x01", 2);
		header->filesize_be1 = htobe32(size);
		header->filesize_be2 = htobe32(size - 0x7000 - 4);
		header->filesize_be3 = htobe32(size);

		/* Control bytes and checksums */
		header->control1 = size + 0x41;
		header->control2 = size + 0xb8;
		header->checksum = htobe16(checksum_g3a(gxa, size));
		header->checksum_2 = htobe32(checksum_g3a_2(gxa, size));

		/* Last 4 bytes */
		uint32_t *footer = gxa + size - 4;
		*footer = header->checksum_2;
	}
}

void edit_name(void *gxa, const char *name)
{
	if(is_g1a(gxa)) {
		memset(G1A(gxa)->header.name, 0, 8);
		if(!name) return;

		for(int i = 0; name[i] && i < 8; i++)
			G1A(gxa)->header.name[i] = name[i];
	}
	else if(is_g3a(gxa)) {
		memset(G3A(gxa)->header.name, 0, 16);
		if(!name) return;

		for(int i = 0; name[i] && i < 16; i++)
			G3A(gxa)->header.name[i] = name[i];

		for(int j = 0; j < 8; j++) {
			memset(G3A(gxa)->header.label[j], 0, 24);
			for(int i = 0; name[i] && i < 24; i++)
				G3A(gxa)->header.label[j][i] = name[i];
		}
	}
}

void edit_internal(void *gxa, const char *internal)
{
	char *dst = NULL;
	int size = 0;

	if(is_g1a(gxa)) {
		dst = G1A(gxa)->header.internal;
		size = 8;
	}
	else if(is_g3a(gxa)) {
		dst = G3A(gxa)->header.internal;
		size = 11;
	}

	memset(dst, 0, size);
	if(!internal) return;

	for(int i = 0; internal[i] && i < size; i++)
		dst[i] = internal[i];
}

void edit_version(void *gxa, const char *version)
{
	char *dst = NULL;
	int size = 10;

	if(is_g1a(gxa))
		dst = G1A(gxa)->header.version;
	else if(is_g3a(gxa))
		dst = G3A(gxa)->header.version;

	memset(dst, 0, size);
	if(!version) return;

	for(int i = 0; version[i] && i < size; i++)
		dst[i] = version[i];
}

void edit_date(void *gxa, const char *date)
{
	char *dst = NULL;
	int size = 14;

	if(is_g1a(gxa))
		dst = G1A(gxa)->header.date;
	else if(is_g3a(gxa))
		dst = G3A(gxa)->header.date;

	memset(dst, 0, size);
	if(!date) return;

	for(int i = 0; date[i] && i < size; i++)
		dst[i] = date[i];
}

void edit_g1a_icon(struct g1a *g1a, uint8_t const *mono)
{
	memcpy(g1a->header.icon, mono, 68);
}

void edit_g3a_icon(struct g3a *g3a, uint16_t const *icon, bool selected)
{
	if(selected)
		memcpy(g3a->header.icon_sel, icon, 92*64*2);
	else
		memcpy(g3a->header.icon_uns, icon, 92*64*2);
}
