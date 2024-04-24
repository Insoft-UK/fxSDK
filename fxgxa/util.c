#include <string.h>
#include <ctype.h>
#include <endianness.h>
#include <fxgxa.h>

bool is_g1a(void *gxa)
{
	/* Check the byte at offset 8 for file type 0xf3 */
	uint8_t mcs_type = ((uint8_t *)gxa)[8];
	return mcs_type == 0xf3;
}

bool is_g3a(void *gxa)
{
	/* Check the byte at offset 8 for file type 0x2c */
	uint8_t mcs_type = ((uint8_t *)gxa)[8];
	return mcs_type == 0x2c;
}

uint16_t word_sum(void const *gxa, size_t offset, int words, size_t size)
{
	uint16_t shorts[words];
	memset(shorts, 0, 2*words);

	/* Extract up to [2*words] bytes from the file */
	int available = (int)size - offset;
	if(available < 0) available = 0;
	if(available > 2*words) available = 2*words;
	memcpy(shorts, gxa+offset, available);

	/* Do the big-endian sum */
	uint16_t sum = 0;
	for(int i = 0; i < words; i++)
		sum += htobe16(shorts[i]);

	return sum;
}

uint16_t checksum_g1a(struct g1a const *g1a, size_t size)
{
	return word_sum(g1a, 0x300, 8, size);
}

uint16_t checksum_g3a(struct g3a const *g3a, size_t size)
{
	return word_sum(g3a, 0x7100, 8, size);
}

uint32_t checksum_g3a_2(struct g3a const *g3a, size_t size)
{
	uint32_t sum = 0;
	uint8_t *data = (void *)g3a;

	for(size_t i = 0; i < 0x20; i++)
		sum += (data[i] ^ 0xff);
	for(size_t i = 0x24; i < size - 4; i++)
		sum += data[i];

	return sum;
}

void default_output(const char *name, const char *suffix, char *output)
{
	/* Check if there is a dot at the end of @name, before the last '/'.
	   The dot must also not be in first position (hidden files) */
	size_t end = strlen(name) - 1;
	while(end >= 1 && name[end] != '/' && name[end] != '.') end--;

	/* If we don't have a dot in the file name, append the extension */
	if(end < 1 || name[end] != '.')
	{
		strcpy(output, name);
		strcat(output, suffix);
	}

	/* If we found a dot before the last slash, replace the extension */
	else
	{
		memcpy(output, name, end);
		strcpy(output + end, suffix);
	}
}

void default_internal(const char *name, char *output, size_t size)
{
	output[0] = '@';
	size_t i = 1;

	for(int j = 0; name[j] && i < size; j++)
	{
		if(isalpha(name[j])) output[i++] = toupper(name[j]);
	}

	output[i] = 0;
}
