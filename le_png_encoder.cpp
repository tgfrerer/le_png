#include "le_png.h"
#include "le_log.h"
#include "le_core.h"

#include "private/le_renderer/le_renderer_types.h"
#include "shared/interfaces/le_image_encoder_interface.h"

#include <cassert>
#include <vector>
#include <string>

#include "lodepng.h"

#if defined( LE_USE_FPNGE )
#	include "fpnge.h"
#endif

struct le_image_encoder_format_o {
	le::Format format; // todo: how can we make sure that the format is srgb if needed?
};

static auto logger = LeLog( "le_png" );

// ----------------------------------------------------------------------
// We must give clients of this encoder a chance to check whether they can assume
// a compatible version of this encoder:
static uint64_t le_image_encoder_get_encoder_version( le_image_encoder_o* encoder ) {
	static constexpr uint64_t ENCODER_VERSION = 0ull << 48 | 0ull << 32 | 1ull << 16 | 0ull << 0;
	return 0;
};

// ----------------------------------------------------------------------

static le_png_image_encoder_parameters_t get_default_parameters() {
	using ns = le_png_image_encoder_parameters_t;
	return le_png_image_encoder_parameters_t{
		// empty for now
	};
}

// ----------------------------------------------------------------------

struct le_image_encoder_o {
	uint32_t image_width  = 0;
	uint32_t image_height = 0;

	std::string output_file_name;

	le_png_image_encoder_parameters_t params = get_default_parameters(); // todo: set defaults
};

// ----------------------------------------------------------------------

static le_image_encoder_o* le_image_encoder_create( char const* file_path, uint32_t width, uint32_t height ) {
	auto self = new le_image_encoder_o();

	self->output_file_name = file_path;
	self->image_width      = width;
	self->image_height     = height;

	return self;
}

// ----------------------------------------------------------------------

static void le_image_encoder_destroy( le_image_encoder_o* self ) {
	delete self;
}

// ----------------------------------------------------------------------

static void le_image_encoder_set_encode_parameters( le_image_encoder_o* self, void* p_parameters ) {
	if ( p_parameters ) {
		self->params = *static_cast<le_png_image_encoder_parameters_t*>( p_parameters );
	} else {
		logger.warn( "Could not set parameters for encoder: Parameters pointer was NULL." );
	}
}

// ----------------------------------------------------------------------

static bool le_image_encoder_write_pixels( le_image_encoder_o* self, uint8_t const* p_pixel_data, size_t pixel_data_byte_count, le_image_encoder_format_o* pixel_data_format ) {

	LodePNGColorType colortype    = LCT_MAX_OCTET_VALUE;
	uint32_t         bitdepth     = 0;
	uint32_t         num_channels = 0;

	switch ( pixel_data_format->format ) {
	case le::Format::eR8Unorm:
		colortype    = LCT_GREY;
		bitdepth     = 8;
		num_channels = 1;
		break;
	case le::Format::eR16Unorm:
		colortype    = LCT_GREY;
		bitdepth     = 16;
		num_channels = 1;
		break;
	case le::Format::eR8G8Unorm:
		colortype    = LCT_GREY_ALPHA;
		bitdepth     = 8;
		num_channels = 2;
		break;
	case le::Format::eR16G16Unorm:
		colortype    = LCT_GREY_ALPHA;
		bitdepth     = 16;
		num_channels = 2;
		break;
	case le::Format::eR8G8B8Unorm:
		colortype    = LCT_RGB;
		bitdepth     = 8;
		num_channels = 3;
		break;
	case le::Format::eR16G16B16Unorm:
		colortype    = LCT_RGB;
		bitdepth     = 16;
		num_channels = 3;
		break;
	case le::Format::eR8G8B8A8Unorm:
	case le::Format::eR8G8B8A8Srgb: // Todo: we must detect non-linear here: set a flag if sRGB
		colortype    = LCT_RGBA;
		bitdepth     = 8;
		num_channels = 4;
		break;
	case le::Format::eR16G16B16A16Unorm:
		colortype    = LCT_RGBA;
		bitdepth     = 16;
		num_channels = 4;
		break;
	default:
		logger.error( "Could not find adapter for requested format %s", to_str( pixel_data_format->format ) );
		return false;
	}

	LodePNGState state;
	lodepng_state_init( &state );

	state.info_raw.colortype       = colortype; // this must match the raw data in p_pixel_data
	state.info_raw.bitdepth        = bitdepth;  // this must match the raw data in p_pixel_data
	state.info_png.color.colortype = colortype; // expose this if you want to allow users to convert images on save
	state.info_png.color.bitdepth  = bitdepth;  // expose this if you want to allow users to convert images on save

	uint8_t* buffer;
	size_t   buffersize;
	uint32_t result = 0;

#ifdef LE_USE_FPNGE
	struct FPNGEOptions options;
	FPNGEFillOptions( &options, 4, FPNGE_CICP_NONE );

	size_t bytes_per_channel = bitdepth / 8;
	buffer                   = ( uint8_t* )malloc( FPNGEOutputAllocSize( bytes_per_channel, num_channels, self->image_width, self->image_height ) );

	buffersize = FPNGEEncode( bytes_per_channel, num_channels, p_pixel_data, self->image_width,
	                          self->image_width * num_channels * bytes_per_channel, self->image_height,
	                          buffer, &options );
#else
	// NOTE: this will allocate memory for buffer - you must free buffer manually.
	result = lodepng_encode( &buffer, &buffersize, p_pixel_data, self->image_width, self->image_height, &state );
	result = state.error;
#endif

	lodepng_state_cleanup( &state );

	if ( 0 == result ) {
		result = lodepng_save_file( buffer, buffersize, self->output_file_name.c_str() );
	}

	free( buffer );

	return result == 0;
}

// ----------------------------------------------------------------------

void* le_image_encoder_clone_parameters_object( void* obj ) {
	auto result = new le_png_image_encoder_parameters_t{
	    *static_cast<le_png_image_encoder_parameters_t*>( obj ) };
	return result;
};

// ----------------------------------------------------------------------
void le_image_encoder_destroy_parameters_object( void* obj ) {
	le_png_image_encoder_parameters_t* typed_obj =
	    static_cast<le_png_image_encoder_parameters_t*>( obj );
	delete ( typed_obj );
};

// ----------------------------------------------------------------------

void le_register_png_encoder_api( void* api ) {

	auto& le_image_encoder_i = static_cast<le_png_api*>( api )->le_png_image_encoder_i;

	if ( le_image_encoder_i == nullptr ) {
		le_image_encoder_i = new le_image_encoder_interface_t{};
	} else {
		// The interface already existed - we have been reloaded and only just need to update
		// function pointer addresses.
		//
		// This is important as by not re-allocating a new interface object
		// but by updating the existing interface object by-value, we keep the *public
		// address for the interface*, while updating its function pointers.
		*le_image_encoder_i = le_image_encoder_interface_t();
	}

	le_image_encoder_i->clone_image_encoder_parameters_object   = le_image_encoder_clone_parameters_object;
	le_image_encoder_i->destroy_image_encoder_parameters_object = le_image_encoder_destroy_parameters_object;

	le_image_encoder_i->create_image_encoder  = le_image_encoder_create;
	le_image_encoder_i->destroy_image_encoder = le_image_encoder_destroy;
	le_image_encoder_i->write_pixels          = le_image_encoder_write_pixels;
	le_image_encoder_i->set_encode_parameters = le_image_encoder_set_encode_parameters;
	le_image_encoder_i->get_encoder_version   = le_image_encoder_get_encoder_version;
}
