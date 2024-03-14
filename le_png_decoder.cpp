#include "le_png.h"
#include "le_log.h"
#include "le_core.h"

#include "private/le_renderer/le_renderer_types.h"
#include "shared/interfaces/le_image_decoder_interface.h"

#include "lodepng.h"
#include <cassert>

struct le_image_decoder_format_o {
	le::Format format;
};

static auto logger = LeLog( "le_png" );

// ----------------------------------------------------------------------

struct le_image_decoder_o {
	uint32_t      image_width  = 0;
	uint32_t      image_height = 0;
	LodePNGState* png_state{};
	uint8_t*      encoded_bytes = nullptr;
	size_t        encoded_bytes_count;

	le::Format image_inferred_format;
	le::Format image_requested_format = le::Format::eUndefined; // requested format wins over inferred format
};

// ----------------------------------------------------------------------

static void le_image_decoder_destroy( le_image_decoder_o* self ) {
	if ( self->encoded_bytes ) {
		free( self->encoded_bytes );
		self->encoded_bytes       = nullptr;
		self->encoded_bytes_count = 0;
	}
	if ( self->png_state ) {
		lodepng_state_cleanup( self->png_state );
	}
	delete self;
}

// ----------------------------------------------------------------------
// This is where we typically load the image metadata
// but we don't need to decode the image yet.
//
static le_image_decoder_o* le_image_decoder_create( char const* file_path ) {
	auto self = new le_image_decoder_o();

	self->png_state = new LodePNGState{};
	lodepng_state_init( self->png_state );

	// todo: check that file exists

	uint32_t result = lodepng_load_file( &self->encoded_bytes, &self->encoded_bytes_count, file_path );

	if ( result != 0 ) {
		logger.error( "Could not load file: '%s', lodepng reported error number: %d", file_path, result );
		le_image_decoder_destroy( self );
		return nullptr;
	}

	//---------- | Invariant: image decode did work

	// Get file info from file without decoding the file immediately.

	lodepng_inspect( &self->image_width, &self->image_height, self->png_state, self->encoded_bytes, self->encoded_bytes_count );

	// typedef enum LodePNGColorType {
	//   LCT_GREY = 0, /*grayscale: 1,2,4,8,16 bit*/
	//   LCT_RGB = 2, /*RGB: 8,16 bit*/
	//   LCT_PALETTE = 3, /*palette: 1,2,4,8 bit*/
	//   LCT_GREY_ALPHA = 4, /*grayscale with alpha: 8,16 bit*/
	//   LCT_RGBA = 6, /*RGB with alpha: 8,16 bit*/
	// } LodePNGColorType;

	// set image inferred format

	uint32_t bit_depth = self->png_state->info_png.color.bitdepth;

	switch ( self->png_state->info_png.color.colortype ) {
	case LodePNGColorType::LCT_GREY:
		if ( bit_depth == 8 ) {
			self->image_inferred_format = le::Format::eR8Unorm;
		} else if ( bit_depth == 16 ) {
			self->image_inferred_format = le::Format::eR16Unorm;
		} else {
			goto cleanup;
		}
		break;
	case LodePNGColorType::LCT_GREY_ALPHA:
		if ( bit_depth == 8 ) {
			self->image_inferred_format = le::Format::eR8G8Unorm;
		} else if ( bit_depth == 16 ) {
			self->image_inferred_format = le::Format::eR16G16Unorm;
		} else {
			goto cleanup;
		}
		break;
	case LodePNGColorType::LCT_RGB:
		if ( bit_depth == 8 ) {
			self->image_inferred_format = le::Format::eR8G8B8Unorm;
		} else if ( bit_depth == 16 ) {
			self->image_inferred_format = le::Format::eR16G16B16Unorm;
		} else {
			goto cleanup;
		}
		break;
	case LodePNGColorType::LCT_RGBA:
		if ( bit_depth == 8 ) {
			self->image_inferred_format = le::Format::eR8G8B8A8Unorm;
		} else if ( bit_depth == 16 ) {
			self->image_inferred_format = le::Format::eR16G16B16A16Unorm;
		} else {
			goto cleanup;
		}
		break;

	default:
		goto cleanup;
	}

	return self;

	// I know this looks terrible, but it appears to be the cleanest way to make
	// sure that things are cleaned up - forgive me, Mr. Dijkstra...

cleanup:

	logger.error( "Unsupported png colour type and bitdepth compination: %d:%d",
	              self->png_state->info_png.color.colortype,
	              self->png_state->info_png.color.bitdepth );
	le_image_decoder_destroy( self );
	return nullptr;
}

// ----------------------------------------------------------------------
// read pixels from file into pixel_data
static bool le_image_decoder_read_pixels( le_image_decoder_o* self, uint8_t* pixel_data, size_t pixels_byte_count ) {

	// We must adjust how we read our bytes based on the requested format.
	// but we could just as well exit early

	// we must match from requested format to the closest matching target format
	// typedef enum LodePNGColorType {
	//   LCT_GREY = 0, /*grayscale: 1,2,4,8,16 bit*/
	//   LCT_RGB = 2, /*RGB: 8,16 bit*/
	//   LCT_PALETTE = 3, /*palette: 1,2,4,8 bit*/
	//   LCT_GREY_ALPHA = 4, /*grayscale with alpha: 8,16 bit*/
	//   LCT_RGBA = 6, /*RGB with alpha: 8,16 bit*/
	// } LodePNGColorType;
	auto format = ( self->image_requested_format != le::Format::eUndefined ) ? self->image_requested_format : self->image_inferred_format;

	uint32_t num_channels = 0;

	switch ( format ) {
	case le::Format::eR8Unorm:
		self->png_state->info_raw.colortype = LCT_GREY;
		self->png_state->info_raw.bitdepth  = 8;
		num_channels                        = 1;
		break;
	case le::Format::eR16Unorm:
		self->png_state->info_raw.colortype = LCT_GREY;
		self->png_state->info_raw.bitdepth  = 16;
		num_channels                        = 1;
		break;
	case le::Format::eR8G8Unorm:
		self->png_state->info_raw.colortype = LCT_GREY_ALPHA;
		self->png_state->info_raw.bitdepth  = 8;
		num_channels                        = 2;
		break;
	case le::Format::eR16G16Unorm:
		self->png_state->info_raw.colortype = LCT_GREY_ALPHA;
		self->png_state->info_raw.bitdepth  = 16;
		num_channels                        = 2;
		break;
	case le::Format::eR8G8B8Unorm:
		self->png_state->info_raw.colortype = LCT_RGB;
		self->png_state->info_raw.bitdepth  = 8;
		num_channels                        = 3;
		break;
	case le::Format::eR16G16B16Unorm:
		self->png_state->info_raw.colortype = LCT_RGB;
		self->png_state->info_raw.bitdepth  = 16;
		num_channels                        = 3;
		break;
	case le::Format::eR8G8B8A8Unorm:
		self->png_state->info_raw.colortype = LCT_RGBA;
		self->png_state->info_raw.bitdepth  = 8;
		num_channels                        = 4;
		break;
	case le::Format::eR16G16B16A16Unorm:
		self->png_state->info_raw.colortype = LCT_RGBA;
		self->png_state->info_raw.bitdepth  = 16;
		num_channels                        = 4;
		break;
	default:
		logger.error( "Could not find adapter for requested format %s", to_str( format ) );
		return false;
	}

	// we must make sure that there is enough space in the output buffer
	size_t required_bytes_count =
	    num_channels *
	    ( self->png_state->info_raw.bitdepth / 8 ) // Note that we convert bit-depth into byte depth here
	    * self->image_width * self->image_height;

	if ( pixels_byte_count == required_bytes_count ) {
		//
		// This is really annoying - instead of directly writing into memory that we allocate,
		// this api will allocate into a buffer that it owns, and will then have us clean up
		// that buffer (tmp_data)...
		//
		// What we would have wanted is a situation where we provide some data to write into,
		// and a size for the data that was allocated, and we then invite lodepng to decode (write)
		// the image into.
		//
		unsigned char* tmp_data = nullptr;
		uint32_t       result   = lodepng_decode( &tmp_data, &self->image_width, &self->image_height, self->png_state, self->encoded_bytes, self->encoded_bytes_count );
		memcpy( pixel_data, tmp_data, pixels_byte_count );
		free( tmp_data );

		if ( result != 0 ) {
			logger.error( "lodepng reported decode error: %d", result );
		}

	} else {
		logger.error( "pixels_byte_count does not match number of requested bytes: %d != %d", pixels_byte_count, required_bytes_count );
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------

static void le_image_decoder_get_image_data_description( le_image_decoder_o* self, le_image_decoder_format_o* p_format, uint32_t* w, uint32_t* h ) {
	if ( p_format ) {
		p_format->format = ( self->image_requested_format != le::Format::eUndefined ) ? self->image_requested_format : self->image_inferred_format;
	}
	if ( w ) {
		*w = self->image_width;
	}
	if ( h ) {
		*h = self->image_height;
	}
};

// ----------------------------------------------------------------------

static void le_image_decoder_set_requested_format( le_image_decoder_o* self, le_image_decoder_format_o const* format ) {
	self->image_requested_format = format->format;
}

// ----------------------------------------------------------------------

void le_register_png_decoder_api( void* api ) {

	auto& le_image_decoder_i = static_cast<le_png_api*>( api )->le_png_image_decoder_i;

	if ( le_image_decoder_i == nullptr ) {
		le_image_decoder_i = new le_image_decoder_interface_t{};
	} else {
		// The interface already existed - we have been reloaded and only just need to update
		// function pointer addresses.
		//
		// This is important as by not re-allocating a new interface object
		// but by updating the existing interface object by-value, we keep the *public
		// address for the interface*, while updating its function pointers.
		*le_image_decoder_i = le_image_decoder_interface_t();
	}

	le_image_decoder_i->create_image_decoder       = le_image_decoder_create;
	le_image_decoder_i->destroy_image_decoder      = le_image_decoder_destroy;
	le_image_decoder_i->read_pixels                = le_image_decoder_read_pixels;
	le_image_decoder_i->get_image_data_description = le_image_decoder_get_image_data_description;
	le_image_decoder_i->set_requested_format       = le_image_decoder_set_requested_format;
}
