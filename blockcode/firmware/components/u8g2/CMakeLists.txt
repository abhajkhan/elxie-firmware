# U8g2 graphics library (olikraus/u8g2, BSD-2-Clause), vendored so the build
# needs no download. Fonts trimmed to the four the face uses (csrc/u8g2_fonts.c).
file(GLOB U8G2_SRCS "${CMAKE_CURRENT_LIST_DIR}/csrc/*.c")
idf_component_register(SRCS ${U8G2_SRCS} INCLUDE_DIRS "csrc")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable -Wno-unused-function)
