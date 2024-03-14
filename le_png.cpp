#include "le_png.h"

// ----------------------------------------------------------------------
extern void le_register_png_decoder_api( void* ); // in le_png_decoder.cpp
extern void le_register_png_encoder_api( void* ); // in le_png_encoder.cpp

LE_MODULE_REGISTER_IMPL( le_png, api ) {

	le_register_png_decoder_api( api );
	le_register_png_encoder_api( api );
}
