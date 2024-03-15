#ifndef GUARD_le_png_H
#define GUARD_le_png_H

#include "le_core.h"

// The decoder interface is rarely used direcly. You are probably better off using
// `le_resource_manager`.
//
// The decoder interface is declared in:
//
// #include "shared/interfaces/le_image_decoder_interface.h"
//
// The encoder interface is declared in:
//
// #include "shared/interfaces/le_image_encoder_interface.h"

struct le_image_decoder_interface_t;
struct le_image_encoder_interface_t;

struct le_png_image_encoder_parameters_t {
	// parameters are empty for now - we might want to allow
	// choosing the number of channels, and the compression ratio here
	// in the future.
};

// clang-format off
struct le_png_api {
    le_image_decoder_interface_t * le_png_image_decoder_i = nullptr; // abstract image decoder interface
    le_image_encoder_interface_t * le_png_image_encoder_i = nullptr; // abstract image encoder interface
};
// clang-format on

LE_MODULE( le_png );
LE_MODULE_LOAD_DEFAULT( le_png );

#ifdef __cplusplus

namespace le_png {
static const auto& api = le_png_api_i;
} // namespace le_png

#endif // __cplusplus

#endif
