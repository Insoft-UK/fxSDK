"""
Convert data files into gint formats or object files
"""

import os
import tempfile
import subprocess
import collections
import fnmatch
import re

from PIL import Image

__all__ = [
	# Color names
	"FX_BLACK", "FX_DARK", "FX_LIGHT", "FX_WHITE", "FX_ALPHA",
	# Conversion mechanisms
	"ObjectData", "u8", "u16", "u32", "ref", "sym",
	# Functions
	"quantize", "convert", "elf",
	# Reusable classes
	"Area", "Grid",
	# Reusable converters
	"convert_bopti_fx", "convert_bopti_cg",
	"convert_topti",
	"convert_libimg_fx", "convert_libimg_cg",
	# Meta API to use fxconv-metadata.txt files
	"Metadata", "parse_parameters",
]

#
#  Constants
#

# Colors
FX_BLACK  = (  0,   0,   0, 255)
FX_DARK   = ( 85,  85,  85, 255)
FX_LIGHT  = (170, 170, 170, 255)
FX_WHITE  = (255, 255, 255, 255)
FX_ALPHA  = (  0,   0,   0,   0)

# fx-9860G profiles
class FxProfile:
	def __init__(self, id, name, colors, layers):
		"""
		Construct an FxProfile object.
		* [id] is the profile ID in bopti
		* [name] is the profile's name as seen in the "profile" key
		* [colors] is the set of supported colors
		* [layers] is a list of layer functions
		"""

		self.id = id
		self.name = name
		self.gray = FX_LIGHT in colors or FX_DARK in colors
		self.colors = colors
		self.layers = layers

	@staticmethod
	def find(name):
		"""Find a profile by name."""
		for profile in FX_PROFILES:
			if profile.name == name:
				return profile
		return None

FX_PROFILES = [
	# Usual black-and-white bitmaps without transparency, as in MonochromeLib
	FxProfile(0x0, "mono", { FX_BLACK, FX_WHITE }, [
		lambda c: (c == FX_BLACK),
	]),
    # Black-and-white with transparency, equivalent of two bitmaps in ML
	FxProfile(0x1, "mono_alpha", { FX_BLACK, FX_WHITE, FX_ALPHA }, [
		lambda c: (c != FX_ALPHA),
		lambda c: (c == FX_BLACK),
	]),
	# Gray engine bitmaps, reference could have been Eiyeron's Gray Lib
	FxProfile(0x2, "gray", { FX_BLACK, FX_DARK, FX_LIGHT, FX_WHITE }, [
		lambda c: (c in [FX_BLACK, FX_LIGHT]),
		lambda c: (c in [FX_BLACK, FX_DARK]),
	]),
	# Gray images with transparency, unfortunately 3 layers since 5 colors
	FxProfile(0x3, "gray_alpha",
		{ FX_BLACK, FX_DARK, FX_LIGHT, FX_WHITE, FX_ALPHA }, [
		lambda c: (c != FX_ALPHA),
		lambda c: (c in [FX_BLACK, FX_LIGHT]),
		lambda c: (c in [FX_BLACK, FX_DARK]),
	]),
]

# fx-CG 50 profiles
class CgProfile:
	def __init__(self, id, depth, names, alpha=None, palette=None):
		# Numerical ID
		self.id = id
		# Name for printing
		self.names = names
		# Bit depth
		self.depth = depth
		# Whether the profile has alpha
		self.has_alpha = (alpha is not None)
		# Alpha value (None has_alpha == False)
		self.alpha = alpha
		# Whether the profile is indexed
		self.is_indexed = (palette is not None)

		if palette is not None:
			# Palette base value (skipping the alpha value)
			self.palette_base = palette[0]
			# Color count (indices are always 0..color_count-1, wraps around)
			self.color_count = palette[1]
			# Whether to trim the palette to a minimal length
			self.trim_palette = palette[2]

	@staticmethod
	def find(name):
		"""Find a profile by name."""
		for profile in CG_PROFILES:
			if name in profile.names:
				return profile
		return None

IMAGE_RGB16 = 0
IMAGE_P8 = 1
IMAGE_P4 = 2

CG_PROFILES = [
	# 16-bit RGB565 and RGB565 with alpha
	CgProfile(0x0, IMAGE_RGB16, ["rgb565", "r5g6b5"]),
	CgProfile(0x1, IMAGE_RGB16, ["rgb565a", "r5g6b5a"], alpha=0x0001),
	# 8-bit palette for RGB565 and RGB565A
	CgProfile(0x4, IMAGE_P8, "p8_rgb565", palette=(0x80,256,True)),
	CgProfile(0x5, IMAGE_P8, "p8_rgb565a", alpha=0x80, palette=(0x81,256,True)),
	# 4-bit palette for RGB565 and RGB565A
	CgProfile(0x6, IMAGE_P4, "p4_rgb565", palette=(0,16,False)),
	CgProfile(0x3, IMAGE_P4, "p4_rgb565a", alpha=0, palette=(1,16,False)),
]

# Libimg flags
LIBIMG_FLAG_OWN = 1
LIBIMG_FLAG_RO  = 2

#
#  Character sets
#

FX_CHARSETS = {
	# Digits 0...9
	"numeric":  [ (ord('0'), 10) ],
	# Uppercase letters A...Z
	"upper":    [ (ord('A'), 26) ],
	# Upper and lowercase letters A..Z, a..z
	"alpha":    [ (ord('A'), 26), (ord('a'), 26) ],
	# Letters and digits A..Z, a..z, 0..9
	"alnum":    [ (ord('A'), 26), (ord('a'), 26), (ord('0'), 10) ],
	# All printable characters from 0x20 to 0x7e
	"print":    [ (0x20, 95) ],
	# All 128 ASCII characters
	"ascii":    [ (0x00, 128) ],
	# Custom Unicode block intervals
	"unicode":  [],
    # Single block 0x00-0xff (does not imply single-byte encoding)
	"256chars": [ (0x00, 256) ],
}

#
# Conversion mechanisms
#

def u8(x):
    return bytes([ x & 255 ])
def u16(x):
    return bytes([ (x >> 8) & 255, x & 255 ])
def u32(x):
    return bytes([ (x >> 24) & 255, (x >> 16) & 255, (x >> 8) & 255, x & 255 ])

def ref(base, offset=None, padding=None, prefix_underscore=True):
	if isinstance(base, bytes) or isinstance(base, bytearray):
		base = bytes(base)
		if offset is not None:
			raise FxconvError(f"reference to bytes does not allow offset")
		if padding and len(base) % padding != 0:
			base += bytes(padding - len(base) % padding)
		return Ref("bytes", base)

	elif isinstance(base, str):
		if padding is not None:
			raise FxconvError(f"reference to name does not allow padding")
		if prefix_underscore:
			base = "_" + base
		if offset is not None:
			offset = int(offset)
			base = f"{base} + {offset}"
		return Ref("name", base)

	elif isinstance(base, ObjectData):
		if offset is not None or padding is not None:
			raise FxconvError("reference to structure does not allow offset " +
				"or padding")
		return Ref("struct", base)

	else:
		raise FxconvError(f"invalid type {type(base)} for ref()")

def ptr(base):
	return ref(base)

def chars(text, length, require_final_nul=True):
	btext = bytes(text, 'utf-8')
	if len(btext) >= length and require_final_nul:
		raise FxconvError(f"'{text}' does not fit within {length} bytes")
	return btext + bytes(length - len(btext))

def string(text):
	return ref(bytes(text, 'utf-8') + bytes([0]))

def sym(name):
	return Sym("_" + name)

# There are 3 kinds of Refs:
#   "bytes"    -> target is a bytes(), we point to that data
#   "name"     -> target is an str like "_sym+2", we point to that
#   "struct"   -> target is an ObjectData
Ref = collections.namedtuple("Ref", ["kind", "target"])

Sym = collections.namedtuple("Sym", ["name"])

class ObjectData:
	"""
	A sequence of bytes that can contain pointers to external variables or
	other data generated along the output structure.
	"""

	def __init__(self, alignment=4):
		"""Construct an empty ObjectData sequence."""

		if alignment & (alignment - 1) != 0:
			raise FxconvError(f"invalid ObjectData alignment {align} (not a " +
				"power of 2)")
		self.alignment = alignment

		# Elements in the structure: bytes, Ref, Sym, ObjectData
		self.inner = []

	def __add__(self, other):
		if isinstance(other, bytes) or isinstance(other, bytearray):
			self.inner.append(bytes(other))
		elif isinstance(other, Ref):
			self.inner.append(other)
		elif isinstance(other, Sym):
			self.inner.append(other)
		elif isinstance(other, ObjectData):
			self.inner.append(other)
		return self

	@staticmethod
	def element_size(el):
		if isinstance(el, bytes):
			return len(el)
		elif isinstance(el, Ref):
			return 4
		elif isinstance(el, Sym):
			return 0
		elif isinstance(el, tuple): # linked sub-ObjectData
			return el[1]
		else:
			raise Exception(f"invalid _element_length: {el}")

	def align(self, size, alignment, elements):
		padding = (alignment - size) % alignment
		if padding != 0:
			elements.append(bytes(padding))
		return padding

	def link(self, symbol):
		inner = []
		outer = []
		elements = []
		size = 0

		# First unfold all structures within [inner] as we accumulate the total
		# size of the inner data
		for el in self.inner:
			if isinstance(el, ObjectData):
				size += self.align(size, el.alignment, inner)
				code, code_size = el.link(f"{symbol} + {size}")
				inner.append((code, code_size))
				size += code_size
			else:
				inner.append(el)
				size += self.element_size(el)

		# Then replace complex references with unfolded data appended at the
		# end of the structure
		for el in inner:
			if isinstance(el, Ref) and el.kind == "bytes":
				elements.append(Ref("name", f"{symbol} + {size}"))
				outer.append(el.target)
				size += self.element_size(el.target)

			elif isinstance(el, Ref) and el.kind == "struct":
				size += self.align(size, el.target.alignment, outer)
				elements.append(Ref("name", f"{symbol} + {size}"))
				code, code_size = el.target.link(f"{symbol} + {size}")
				outer.append((code, code_size))
				size += code_size

			else:
				elements.append(el)

		elements += outer

		# Make sure the whole structure is properly aligned
		size += self.align(size, self.alignment, elements)

		# Finally, generate actual assembler code based on all elements
		asm = ""

		for el in elements:
			if isinstance(el, bytes):
				asm += ".byte " + ",".join(hex(x) for x in el) + "\n"
			elif isinstance(el, Ref) and el.kind == "name":
				asm += f".long {el.target}\n"
			elif isinstance(el, Sym):
				asm += f".global {el.name}\n"
				asm += f"{el.name}:\n"
			elif isinstance(el, tuple): # linked ObjectData
				asm += el[0]

		return asm, size

# User-friendly synonym
Structure = ObjectData

#
#  Area specifications
#

class Area:
	"""
	A subrectangle of an image, typically used for pre-conversion cropping.
	"""

	def __init__(self, area, img):
		"""
		Construct an Area object from a dict specification. The following keys
		may be used to specific the position and size of the rectangle:

		* "x", "y"          (int strings, default to 0)
		* "width", "height" (int strings, default to image dimensions)
		* "size"            ("WxH" where W and H are the width and height)

		The Area objects has attributes "x", "y", "w" and "h". Both positions
		default to 0 and both sizes to the corresponding image dimensions.
		"""

		self.x = int(area.get("x", 0))
		self.y = int(area.get("y", 0))
		self.w = int(area.get("width", img.width))
		self.h = int(area.get("height", img.height))

		if "size" in area:
			self.w, self.h = map(int, area["size"].split("x"))

	def tuple(self):
		"""Return the tuple representation (x,y,w,h), suitable for .crop(). """
		return (self.x, self.y, self.w, self.h)

#
#  Grid specifications
#

class Grid:
	"""
	A grid over an image, used to isolate glyphs in fonts and tiles in maps.
	Supports several types of spacing. To apply an outer border, use crop
	through an Area before using the Grid.
	"""

	def __init__(self, grid):
		"""
		Construct a Grid object from a dict specification. The following keys
		may be used to specify the dimension and spacing of the cells:

		* "border"           (int string, defaults to 0)
		* "padding"          (int string, defaults to 0)
		* "width", "height"  (int strings, mandatory if "size" not set)
		* "size"             ("WxH" where W and H are the cell width/height)

		The Grid object has attributes "border", "padding", "w" and "h". Each
		cell is of size "(w,h)" and has "padding" pixels of proper padding
		around it. Additionally, cells are separated by a border of size
		"border"; this includes an outer border.
		"""

		self.border = int(grid.get("border", 0))
		self.padding = int(grid.get("padding", 0))

		self.w = int(grid.get("width", -1))
		self.h = int(grid.get("height", -1))

		if "size" in grid:
			self.w, self.h = map(int, grid["size"].split("x"))

		if self.w <= 0 or self.h <= 0:
			raise FxconvError("size of grid unspecified or invalid")

	def size(self, img):
		"""Count the number of elements in the grid."""
		b, p, w, h =  self.border, self.padding, self.w, self.h

		# Padding-extended parameters
		W = w + 2 * p
		H = h + 2 * p

		columns = (img.width  - b) // (W + b)
		rows    = (img.height - b) // (H + b)
		return columns * rows


	def iter(self, img):
		"""Yields subrectangles of the grid as tuples (x,y,w,h)."""
		b, p, w, h =  self.border, self.padding, self.w, self.h

		# Padding-extended parameters
		W = w + 2 * p
		H = h + 2 * p

		columns = (img.width  - b) // (W + b)
		rows    = (img.height - b) // (H + b)

		for r in range(rows):
			for c in range(columns):
				x = b + c * (W + b) + p
				y = b + r * (H + b) + p
				yield (x, y, x + w, y + h)

#
# Binary conversion
#

def convert_binary(input, params):
	return open(input, "rb").read()

#
# Image conversion for fx-9860G
#

def convert_bopti_fx(input, params):
	if isinstance(input, Image.Image):
		img = input.copy()
	else:
		img = Image.open(input)
	if img.width >= 4096 or img.height >= 4096:
		raise FxconvError(f"'{input}' is too large (max. 4095x4095)")

	# Expand area.size and get the defaults. Crop image to resulting area.
	area = Area(params.get("area", {}), img)
	img = img.crop(area.tuple())

	# Quantize the image and check the profile
	img = quantize(img, dither=False)

	# If profile is provided, check its validity, otherwise use the smallest
	# compatible profile

	colors = { y for (x,y) in img.getcolors() }

	if "profile" in params:
		name = params["profile"]
		p = FxProfile.find(name)

		if p is None:
			raise FxconvError(f"unknown profile {name} in '{input}'")
		if colors - p.colors:
			raise FxconvError(f"{name} has too few colors for '{input}'")
	else:
		name = "gray" if FX_LIGHT in colors or FX_DARK in colors else "mono"
		if FX_ALPHA in colors: name += "_alpha"
		p = FxProfile.find(name)

	# Make the image header

	header = bytes ([(0x80 if p.gray else 0) + p.id])
	encode24bit = lambda x: bytes([ x >> 16, (x & 0xff00) >> 8, x & 0xff ])
	header += encode24bit((img.size[0] << 12) + img.size[1])

	# Split the image into layers depending on the profile and zip them all

	layers = [ _image_project(img, layer) for layer in p.layers ]
	count  = len(layers)
	size   = len(layers[0])

	data = bytearray(count * size)
	n = 0

	for longword in range(size // 4):
		for layer in layers:
			for i in range(4):
				data[n] = layer[4 * longword + i]
				n += 1

	# Generate the object file
	return header + data

def _image_project(img, f):
	# New width and height
	w = (img.size[0] + 31) // 32
	h = (img.size[1])

	data = bytearray(4 * w * h)
	im = img.load()

	# Now generate a 32-bit byte sequence
	for y in range(img.size[1]):
		for x in range(img.size[0]):
			bit = int(f(im[x, y]))
			data[4 * y * w + (x >> 3)] |= (bit << (~x & 7))

	return data

#
# Image conversion for fx-CG 50
#

def image_has_alpha(img):
	# Convert the alpha channel to 1-bit; check if there are transparent pixels
	try:
		alpha_channel = img.getchannel("A").convert("1", dither=Image.NONE)
		alpha_levels = { t[1]: t[0] for t in alpha_channel.getcolors() }
		return 0 in alpha_levels
	except ValueError:
		return False

def convert_bopti_cg(input, params):
	return convert_image_cg(input, params)

def convert_image_cg(input, params):
	if isinstance(input, Image.Image):
		img = input.copy()
	else:
		img = Image.open(input)
	if img.width >= 65536 or img.height >= 65536:
		raise FxconvError(f"'{input}' is too large (max. 65535x65535)")

	# Crop image to key "area"
	area = Area(params.get("area", {}), img)
	img = img.crop(area.tuple())

	img = img.convert("RGBA")

	#---
	# Format selection
	#---

	format_name = params.get("profile", "")
	has_alpha = image_has_alpha(img)

	# If no format is specified, select rgb565 or rgb565a
	if format_name == "":
		format_name = "rgb565a" if has_alpha else "rgb565"
	# Similarly, if "p8" or "p4" is specified, select the cheapest variation
	elif format_name == "p8":
		format_name = "p8_rgb565a" if has_alpha else "p8_rgb565"
	elif format_name == "p4":
		format_name = "p4_rgb565a" if has_alpha else "p4_rgb565"

	# Otherwise, just use the format as specified
	format = CgProfile.find(format_name)
	if format is None:
		raise FxconvError(f"unknown image format '{format_name}")

	# Check that we have transparency support if we need it
	if has_alpha and not format.has_alpha:
		raise FxconvError(f"'{input}' has transparency, which {format_name} "+
			"doesn't support")

	#---
	# Structure generation
	#---

	data, stride, palette, color_count = image_encode(img, format)

	o = ObjectData()
	o += u8(format.id)
	o += u8(3) # DATA_RO, PALETTE_RO
	o += u16(color_count)
	o += u16(img.width)
	o += u16(img.height)
	o += u32(stride)
	o += ref(data, padding=4)
	if palette is None:
		o += u32(0)
	else:
		o += ref(palette)
	return o

#
# Font conversion
#

def _trim(img):
	def blank(x):
		return all(px[x,y] == FX_WHITE for y in range(img.height))

	left = 0
	right = img.width
	px = img.load()

	while left + 1 < right and blank(left):
		left += 1
	while right - 1 > left and blank(right - 1):
		right -= 1

	return img.crop((left, 0, right, img.height))

def _blockstart(name):
	m = re.match(r'(?:U\+)?([0-9A-Fa-f]+)\.', name)

	if m is None:
		return None
	try:
		return int(m[1], base=16)
	except Exception as e:
		return None

def convert_topti(input, params):

	#--
	# Character set
	#--

	if "charset" not in params:
		raise FxconvError("'charset' attribute is required and missing")

	charset = params["charset"]
	blocks = FX_CHARSETS.get(charset, None)
	if blocks is None:
		raise FxconvError(f"unknown character set '{charset}'")

	# Will be recomputed later for Unicode fonts
	glyph_count = sum(length for start, length in blocks)

	#--
	# Image input
	#--

	grid = Grid(params.get("grid", {}))

	# When using predefined charsets with a single image, apply the area and
	# check that the number of glyphs is correct
	if charset != "unicode":
		if isinstance(input, Image.Image):
			img = input.copy()
		else:
			img = Image.open(input)
		area = Area(params.get("area", {}), img)
		img = img.crop(area.tuple())

		# Quantize it (only black pixels will be encoded into glyphs)
		img = quantize(img, dither=False)

		if glyph_count > grid.size(img):
			raise FxconvError(
				f"not enough elements in grid (got {grid.size(img)}, "+
				f"need {glyph_count} for '{charset}')")

		inputs = [ img ]

	# In Unicode mode, load images for the provided directory, but don't apply
	# the area (this makes no sense since the sizes are different)
	else:
		try:
			files = os.listdir(input)
		except Exception as e:
			raise FxconvError(
				f"cannot scan directory '{input}' to discover blocks for the"+
				f"unicode charset: {str(e)}")

		# Keep only files with basenames like "<hexa>" or "U+<hexa>" and sort
		# them by code point order (for consistency)
		files = [e for e in files if _blockstart(e) is not None]
		files = sorted(files, key=_blockstart)

		# Open all images and guess the block size
		inputs = []
		for file in files:
			img = Image.open(os.path.join(input, file))
			img = quantize(img, dither=False)
			inputs.append(img)

		blocks = [(_blockstart(e), grid.size(img))
		          for e, img in zip(files, inputs)]

		# Recompute the total glyph count
		glyph_count = sum(length for start, length in blocks)


	#--
	# Proportionality and metadata
	#--

	proportional = (params.get("proportional", "false") == "true")
	title = bytes(params.get("title", ""), "utf-8") + bytes([0])

	flags = set(params.get("flags", "").split(","))
	flags.remove("")
	flags_std = { "bold", "italic", "serif", "mono" }

	if flags - flags_std:
		raise FxconvError(f"unknown flags: {', '.join(flags - flags_std)}")

	bold   = int("bold" in flags)
	italic = int("italic" in flags)
	serif  = int("serif" in flags)
	mono   = int("mono" in flags)

	flags = (bold << 7) | (italic << 6) | (serif << 5) | (mono << 4) \
			| int(proportional)
	# Default line height to glyph height
	line_height = int(params.get("height", grid.h))

	# Default character spacing to 1
	char_spacing = int(params.get("char-spacing", "1"))

	#--
	# Encoding blocks
	#---

	def encode_block(b):
		start, length = b
		return u32((start << 12) | length)

	data_blocks = b''.join(encode_block(b) for b in blocks)

	#--
	# Encoding glyphs
	#--

	data_glyphs = []
	total_glyphs = 0
	data_width = bytearray()
	data_index  = bytearray()

	for img in inputs:
		for (number, region) in enumerate(grid.iter(img)):
			# Upate index
			if not (number % 8):
				idx = total_glyphs // 4
				data_index += u16(idx)

			# Get glyph area
			glyph = img.crop(region)
			if proportional:
				glyph = _trim(glyph)
				data_width.append(glyph.width)

			length = 4 * ((glyph.width * glyph.height + 31) >> 5)
			bits = bytearray(length)
			offset = 0
			px = glyph.load()

			for y in range(glyph.size[1]):
				for x in range(glyph.size[0]):
					color = (px[x,y] == FX_BLACK)
					bits[offset >> 3] |= ((color * 0x80) >> (offset & 7))
					offset += 1

			data_glyphs.append(bits)
			total_glyphs += length

	data_glyphs = b''.join(data_glyphs)

	#---
	#  Object file generation
	#---

	# Base data: always put the raw data and blocks first since they are
	# 4-aligned, to preserve alignment on the rest of the references.
	o = ObjectData()

	# Make the title pointer NULL if no title is specified
	if len(title) > 1:
		o += ref(title, padding=4)
	else:
		o += u32(0)

	o += u8(flags) + u8(line_height) + u8(grid.h) + u8(len(blocks))
	o += u32(glyph_count)
	o += u8(char_spacing) + bytes(3)
	o += ref(data_blocks)
	o += ref(data_glyphs)

	# For proportional fonts, add the index (2-aligned) then the glyph size
	# array (1-aligned).
	if proportional:
		o += ref(data_index)
		o += ref(data_width)
	# For fixed-width fonts, add more metrics
	else:
		o += u16(grid.w)
		o += u16((grid.w * grid.h + 31) >> 5)

	return o

#
# libimg conversion for fx-9860G
#

def convert_libimg_fx(input, params):
	if isinstance(input, Image.Image):
		img = input.copy()
	else:
		img = Image.open(input)
	if img.width >= 65536 or img.height >= 65536:
		raise FxconvError(f"'{input}' is too large (max. 65535x65535)")

	# Crop image to area
	area = Area(params.get("area", {}), img)
	img = img.crop(area.tuple())

	# Quantize the image. We don't need to check if there is gray; the VRAM
	# rendering function for mono output will adjust at runetime
	img = quantize(img, dither=False)
	code = { FX_WHITE: 0, FX_LIGHT: 1, FX_DARK: 2, FX_BLACK: 3, FX_ALPHA: 4 }

	# Encode image as a plain series of pixels
	data = bytearray(img.width * img.height)
	im = img.load()
	i = 0

	for y in range(img.height):
		for x in range(img.width):
			data[i] = code[im[x, y]]
			i += 1

	o = ObjectData()
	o += u16(img.width) + u16(img.height)
	o += u16(img.width) + u8(LIBIMG_FLAG_RO) + bytes(1)
	o += ref(data)

	return o


#
# libimg conversion for fx-CG 50
#

def convert_libimg_cg(input, params):
	if isinstance(input, Image.Image):
		img = input.copy()
	else:
		img = Image.open(input)
	if img.width >= 65536 or img.height >= 65536:
		raise FxconvError(f"'{input}' is too large (max. 65535x65535)")

	# Crop image to key "area"
	area = Area(params.get("area", {}), img)
	img = img.crop(area.tuple())

	# Encode the image into 16-bit format and force the alpha to 0x0001
	format = CgProfile.find("rgb565a")
	encoded, stride, palette, color_count = image_encode(img, format)

	o = ObjectData()
	o += u16(img.width) + u16(img.height)
	o += u16(stride // 2) + u8(LIBIMG_FLAG_RO) + bytes(1)
	o += ref(encoded)

	return o

#
# Exceptions
#

class FxconvError(Exception):
	pass

#
# API
#

def quantize(img, dither=False):
	"""
	Convert a PIL.Image.Image into an RGBA image with only these colors:
	* FX_BLACK  = (  0,   0,   0, 255)
	* FX_DARK   = ( 85,  85,  85, 255)
	* FX_LIGHT  = (170, 170, 170, 255)
	* FX_WHITE  = (255, 255, 255, 255)
	* FX_ALPHA  = (  0,   0,   0,   0)

	The alpha channel is first flattened to either opaque of full transparent,
	then all colors are quantized into the 4-shade scale. Floyd-Steinberg
	dithering can be used, although most applications will prefer nearest-
	neighbor coloring.

	Arguments:
	img     -- Input image, in any format
	dither  -- Enable Floyd-Steinberg dithering [default: False]

	Returns a quantized PIL.Image.Image.
	"""

	# Our palette will have only 4 colors for the gray engine
	colors = [ FX_BLACK, FX_DARK, FX_LIGHT, FX_WHITE ]

	# Create the palette
	palette = Image.new("RGBA", (len(colors), 1))
	for (i, c) in enumerate(colors):
		palette.putpixel((i, 0), c)
	palette = palette.convert("P")

	# Make the image RGBA in case it was indexed so that transparent pixels are
	# represented in an alpha channel
	if img.mode == "P":
		img = img.convert("RGBA")

	# Save the alpha channel, and make it either full transparent or opaque
	try:
		alpha_channel = img.getchannel("A").convert("1", dither=Image.NONE)
	except:
		alpha_channel = Image.new("L", img.size, 255)

	# Apply the palette to the original image (transparency removed)
	img = img.convert("RGB")

	# Let's do an equivalent of the following, but with a dithering setting:
	# img = img.quantize(palette=palette)

	img.load()
	palette.load()
	im = img.im.convert("P", int(dither), palette.im)
	img = img._new(im).convert("RGB")

	# Put back the alpha channel
	img.putalpha(alpha_channel)

	# Premultiply alpha
	pixels = img.load()
	for y in range(img.size[1]):
		for x in range(img.size[0]):
			r, g, b, a = pixels[x, y]
			if a == 0:
				r, g, b, = 0, 0, 0
			pixels[x, y] = (r, g, b, a)

	return img

def image_encode(img, format):
	"""
	Encodes a PIL.Image.Image into one of the fx-CG image formats. The color
	depth is either RGB16, P8 or P4, with various transparency settings and
	palette encodings.

	Returns 4 values:
	* data: A bytearray containing the encoded image data
	* stride: The byte stride of the data array
	* palette: A bytearray containing the encoded palette (None if not indexed)
	* color_count: Number of colors in the palette (-1 if not indexed)
	"""

	#---
	# Separate the alpha channel
	#---

	# Save the alpha channel and make it 1-bit. If there are transparent
	# pixels, set has_alpha=True and record the alpha channel in alpha_pixels.
	if format.has_alpha:
		alpha_channel = img.getchannel("A").convert("1", dither=Image.NONE)
	else:
		alpha_channel = Image.new("1", img.size, 1)

	alpha_pixels = alpha_channel.load()
	img = img.convert("RGB")

	# Transparent pixels have random values on the RGB channels, causing them
	# to use up palette entries during quantization. To avoid that, set their
	# RGB data to a color used somewhere else in the image.
	pixels = img.load()
	bg_color = next((pixels[x,y]
	                 for x in range(img.width) for y in range(img.height)
	                 if alpha_pixels[x,y] > 0),
                    (0,0,0))

	for y in range(img.height):
		for x in range(img.width):
			if alpha_pixels[x,y] == 0:
				pixels[x,y] = bg_color

	#---
	# Quantize to generate a palette
	#---

	if format.is_indexed:
		palette_max_size = format.color_count - int(format.has_alpha)
		img = img.convert("P",
			dither=Image.NONE,
			palette=Image.ADAPTIVE,
			colors=palette_max_size)

		# The palette format is a list of N triplets where N includes both the
		# opaque colors we just generated and an alpha color. Sometimes colors
		# after img.convert() are not numbered 0..N-1, so take the max.
		pixels = img.load()
		N = 1 + max(pixels[x,y]
		            for y in range(img.height) for x in range(img.width))

		palette = img.getpalette()[:3*N]
		palette = list(zip(palette[::3], palette[1::3], palette[2::3]))

		# For formats with transparency, make the transparent color consistent
		if format.has_alpha:
			N += 1
			palette = [(255, 0, 255)] + palette

		# Also keep track of how to remap indices from the values generated by
		# img.convert() into the palette, which is shifted by 1 due to alpha
		# and also starts at format.palette_base. Note: format.palette_base
		# already starts 1 value later for formats with alpha.
		palette_map = [(format.palette_base + i) % format.color_count
		               for i in range(N)]
	else:
		px = img.load()

	#---
	# Encode data into a bytearray
	#---

	def rgb24to16(rgb):
		r = (rgb[0] & 0xff) >> 3
		g = (rgb[1] & 0xff) >> 2
		b = (rgb[2] & 0xff) >> 3
		return (r << 11) | (g << 5) | b

	if format.depth == IMAGE_RGB16:
		# Preserve alignment between rows by padding to 4 bytes
		stride = (img.width + 1) // 2 * 4
		size = stride * img.height
	elif format.depth == IMAGE_P8:
		size = img.width * img.height
		stride = img.width
	elif format.depth == IMAGE_P4:
		# Pad whole bytes
		stride = (img.width + 1) // 2
		size = stride * img.height

	# Encode the pixel data
	data = bytearray(size)

	for y in range(img.height):
		for x in range(img.width):
			a = alpha_pixels[x,y]

			if format.depth == IMAGE_RGB16:
				# If c lands on the alpha value, flip its lowest bit
				c = rgb24to16(pixels[x, y])
				c = format.alpha if (a == 0) else c ^ (c == format.alpha)
				offset = (stride * y) + x * 2
				data[offset:offset+2] = u16(c)

			elif format.depth == IMAGE_P8:
				c = palette_map[pixels[x, y]] if a > 0 else format.alpha
				offset = (stride * y) + x
				data[offset] = c

			elif format.depth == IMAGE_P4:
				c = palette_map[pixels[x, y]] if a > 0 else format.alpha
				offset = (stride * y) + (x // 2)
				if x % 2 == 0:
					data[offset] |= (c << 4)
				else:
					data[offset] |= c

	# Encode the palette

	if format.is_indexed:
		N = N if format.trim_palette else format.color_count
		encoded_palette = bytearray(2 * N)
		for i, rgb24 in enumerate(palette):
			encoded_palette[2*i:2*i+2] = u16(rgb24to16(rgb24))
		return data, stride, encoded_palette, N
	else:
		return data, stride, None, -1

def convert(input, params, target, output=None, model=None, custom=None):
	"""
	Convert a data file into an object that exports the following symbols:
	* _<varname>
	* _<varname>_end
	* _<varname>_size
	The variable name is obtained from the parameter dictionary <params>.

	Arguments:
	input   -- Input file path
	params  -- Parameter dictionary
	target  -- String dictionary keys 'toolchain', 'arch' and 'section'
	output  -- Output file name [default: <input> with suffix '.o']
	model   -- 'fx' or 'cg'  (some conversions require this) [default: None]
	custom  -- Custom conversion function

	Produces an output file and returns nothing.
	"""

	if output is None:
		output = os.path.splitext(input)[0] + ".o"

	if "name" not in params:
		raise FxconvError(f"no name specified for conversion '{input}'")

	if target["arch"] is None:
		target["arch"] = model

	if "custom-type" in params:
		t = params["custom-type"]
		# Also copy it in "type" for older converters (this is legacy)
		params["type"] = t
	elif "type" in params:
		t = params["type"]
	else:
		raise FxconvError(f"missing type in conversion '{input}'")

	if t == "binary":
		o = convert_binary(input, params)
	elif t == "bopti-image" and model in [ "fx", None ]:
		o = convert_bopti_fx(input, params)
	elif t == "bopti-image" and model == "cg":
		o = convert_bopti_cg(input, params)
	elif t == "font":
		o = convert_topti(input, params)
	elif t == "libimg-image" and model in [ "fx", None ]:
		o = convert_libimg_fx(input, params)
	elif t == "libimg-image" and model == "cg":
		o = convert_libimg_cg(input, params)
	elif custom is not None:
		for converter in custom:
			if converter(input, output, params, target) == 0:
				return
		raise FxconvError(f'unknown custom resource type \'{t}\'')
	else:
		raise FxconvError(f'unknown resource type \'{t}\'')

	# Standard conversions: save to ELF in the natural way
	elf(o, output, "_" + params["name"], **target)

def elf(data, output, symbol, toolchain=None, arch=None, section=None,
	assembly=None):
	"""
	Call objcopy to create an object file from the specified data. The object
	file will export three symbols:
	* <symbol>
	* <symbol>_end
	* <symbol>_size

	The symbol name must have a leading underscore if it is to be declared and
	used from a C program.

    The toolchain can be any target triplet for which the compiler is
    available. The architecture is deduced from some typical triplets;
    otherwise it can be set, usually as "sh3" or "sh4-nofpu". This affects the
	--binary-architecture flag of objcopy. If arch is set to "fx" or "cg", this
	function tries to be smart and:

	* Uses the name of the compiler if it contains a full architecture name
	  such as "sh3", "sh4" or "sh4-nofpu";
	* Uses "sh3" for fx9860g and "sh4-nofpu" for fxcg50 if the toolchain is
	  "sh-elf", which is a custom set;
	* Fails otherwise.

	The section name can be specified, along with its flags. A typical example
	would be section=".rodata,contents,alloc,load,readonly,data", which is the
	default.

	If assembly is set to a non-empty assembly program, this function also
	generates a temporary ELF file by assembling this piece of code, and merges
	it into the original one.

	Arguments:
	data       -- A bytes-like or ObjectData object to embed into the output
	output     -- Name of output file
	symbol     -- Chosen symbol name
	toolchain  -- Target triplet [default: "sh3eb-elf"]
	arch       -- Target architecture [default: try to guess]
	section    -- Target section [default: above variation of .rodata]
	assembly   -- Additional assembly code [default: None]

	Produces an output file and returns nothing.
	"""

	# Toolchain parameters

	if toolchain is None:
		toolchain = "sh3eb-elf"
	if section is None:
		section = ".rodata,contents,alloc,load,readonly,data"

	if arch in ["fx", "cg", None] and toolchain in ["sh3eb-elf", "sh4eb-elf",
		"sh4eb-nofpu-elf"]:
		arch = toolchain.replace("eb-", "-")[:-4]

	elif arch == "fx" and toolchain == "sh-elf":
		arch = "sh3"
	elif arch == "cg" and toolchain == "sh-elf":
		arch = "sh4-nofpu"

	elif arch in ["fx", "cg", None]:
		raise FxconvError(f"non-trivial architecture for {toolchain} must be "+
			"specified")

	# Unfold ObjectData into data and assembly
	if isinstance(data, ObjectData):
		asm =   ".section " + section.split(",",1)[0] + "\n"
		asm += f".global {symbol}\n"
		asm += f"{symbol}:\n"
		asm += data.link(symbol)[0]
		asm += (assembly or "")

		data = None
		assembly = asm

	if data is None and assembly is None:
		raise FxconvError("elf() but no data and no assembly")

	# Generate data - in <output> directly if there is no assembly

	if data:
		fp_obj = tempfile.NamedTemporaryFile()
		fp_obj.write(data)
		fp_obj.flush()

		sybl = "_binary_" + fp_obj.name.replace("/", "_")
		objcopy_args = [
			f"{toolchain}-objcopy", "-I", "binary", "-O", "elf32-sh",
			"--binary-architecture", arch, "--file-alignment", "4",
			"--rename-section", f".data={section}",
			"--redefine-sym", f"{sybl}_start={symbol}",
			"--redefine-sym", f"{sybl}_end={symbol}_end",
			"--redefine-sym", f"{sybl}_size={symbol}_size",
			fp_obj.name, output if not assembly else fp_obj.name + ".o" ]

		proc = subprocess.run(objcopy_args)
		if proc.returncode != 0:
			raise FxconvError(f"objcopy returned {proc.returncode}")

	# Generate assembly - in <output> directly if there is no data

	if assembly:
		fp_asm = tempfile.NamedTemporaryFile()
		fp_asm.write(assembly.encode('utf-8'))
		fp_asm.flush()

		as_args = [
			f"{toolchain}-as", "-c", fp_asm.name, "-o",
			output if not data else fp_asm.name + ".o" ]

		proc = subprocess.run(as_args)
		if proc.returncode != 0:
			raise FxconvError(f"as returned {proc.returncode}")

	# If both data and assembly are specified, merge everyone

	if data and assembly:
		ld_args = [
			f"{toolchain}-ld", "-r", fp_obj.name + ".o", fp_asm.name + ".o",
			"-o", output ]

		proc = subprocess.run(ld_args)
		if proc.returncode != 0:
			raise FxconvError(f"ld returned {proc.returncode}")

		os.remove(fp_obj.name + ".o")
		os.remove(fp_asm.name + ".o")

	if data:
		fp_obj.close()
	if assembly:
		fp_asm.close()

#
# Meta API
#

def parse_parameters(params):
	"""Parse parameters of the form "NAME:VALUE" into a dictionary."""
	d = dict()

	def insert(d, path, value):
		if len(path) == 1:
			d[path[0]] = value
		else:
			if not path[0] in d:
				d[path[0]] = dict()
			insert(d[path[0]], path[1:], value)

	for decl in params:
		if ":" not in decl:
			raise FxconvError(f"invalid parameter {decl}, ignoring")
		else:
			name, value = decl.split(":", 1)
			value = value.strip()
			if name == "name_regex":
				value = value.split(" ", 1)
			insert(d, name.split("."), value)

	return d

def _parse_metadata(contents):
	"""
	Parse the contents of an fxconv-metadata.txt file. Comments start with '#'
	anywhere on a line and extend to the end of the line.

	The file is divided in blocks that start with a "<wildcard>:" pattern at
	the first column of a line (no leading spaces) followed by zero or more
	properties declared as "key: value" (with at least one leading space).

	The key can contain dots (eg. "category.field"), in which case the value
	for the main component ("category") is itself a dictionary.
	"""

	RE_COMMENT = re.compile(r'#.*$', re.MULTILINE)
	contents = re.sub(RE_COMMENT, "", contents)

	RE_WILDCARD = re.compile(r'^(\S(?:[^:\s]|\\:|\\ )*)\s*:\s*$', re.MULTILINE)
	lead, *elements = [ s.strip() for s in re.split(RE_WILDCARD, contents) ]

	if lead:
		raise FxconvError(f"invalid metadata: {lead} appears before wildcard")

	# Group elements by pairs (left: wildcard, right: list of properties)
	elements = list(zip(elements[::2], elements[1::2]))

	metadata = []
	for (wildcard, params) in elements:
		params = [ s.strip() for s in params.split("\n") if s.strip() ]
		metadata.append((wildcard, parse_parameters(params)))

	return metadata

class Metadata:
	def __init__(self, path=None, text=None):
		"""
		Load either an fxconv-metadata.txt file (if path is not None) or the
		contents of such a file (if text is not None).
		"""

		if (path is not None) == (text is not None):
			raise ValueError("Metadata must have exactly one of path and text")

		if path is not None:
			self._path = path
			with open(path, "r") as fp:
				self._rules = _parse_metadata(fp.read())
		elif text is not None:
			self._path = None
			self._rules = _parse_metadata(text)

	def path(self):
		"""
		Returns the path of the file from which the metadata was parsed, or
		None if the metadata was parsed from string.
		"""
		return self._path

	def rules(self):
		"""
		Returns a list of pairs (wildcard, rules) where the wildcard is a
		string and the rules are a nested dictionary.
		"""
		return self._rules

	def rules_for(self, path):
		"""
		Returns the parameters that apply to the specified path, or None if no
		wildcard matches it. The special key "name_regex" is also resolved into
		the regular key "name".
		"""

		basename = os.path.basename(path)
		params = dict()
		matched = False

		for (wildcard, p) in self._rules:
			if fnmatch.fnmatchcase(basename, wildcard):
				params.update(**p)
				matched = True

		if not matched:
			return None

		if "name_regex" in params and not "name" in params:
			params["name"] = re.sub(*params["name_regex"], basename)
		return params
