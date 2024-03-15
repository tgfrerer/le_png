set (TARGET le_png)

# list modules this module depends on

depends_on_island_module(le_renderer)
depends_on_island_module(le_log)

option(LE_PNG_ENABLE_FPNGE "Enable fast png encoder" OFF)

if (LE_PNG_ENABLE_FPNGE)
	# this is only needed if you want to use fpnge
	# as the implementation of the fast png encoder
	# depends on hardware CPU SIMD capabilities.
	if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		add_compile_options("-march=native")
		add_compile_definitions("LE_USE_FPNGE")
	endif()
endif()

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
# Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/le_png/lodepng")
    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()


# Source files for this Island module ----

set (SOURCES "le_png.h")
set (SOURCES ${SOURCES} "le_png.cpp")
set (SOURCES ${SOURCES} "le_png_encoder.cpp")
set (SOURCES ${SOURCES} "le_png_decoder.cpp")
set (SOURCES ${SOURCES} "3rdparty/le_png/lodepng/lodepng.cpp")
set (SOURCES ${SOURCES} "3rdparty/le_png/lodepng/lodepng.h")

if (LE_PNG_ENABLE_FPNGE)
	set (SOURCES ${SOURCES} "3rdparty/le_png/fpnge/fpnge.cc")
	set (SOURCES ${SOURCES} "3rdparty/le_png/fpnge/fpnge.h")
endif()

if (${PLUGINS_DYNAMIC})
    add_library(${TARGET} SHARED ${SOURCES})
    add_dynamic_linker_flags()
    target_compile_definitions(${TARGET}  PUBLIC "PLUGINS_DYNAMIC")
else()
    add_library(${TARGET} STATIC ${SOURCES})
    add_static_lib( ${TARGET} )
endif()

# ----------------------------------------------------------------------
# Add include directories -
# Note that these are set as PRIVATE - they apply to the current module.
# ----------------------------------------------------------------------

target_include_directories(${TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/le_png/lodepng")
target_include_directories(${TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/le_png/fpnge")

# ----------------------------------------------------------------------


target_link_libraries(${TARGET} PUBLIC ${LINKER_FLAGS})

source_group(${TARGET} FILES ${SOURCES})
