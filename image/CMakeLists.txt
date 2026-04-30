# Optional default-image static library.
#
# Only entered from the parent CMakeLists when TMF8829_INCLUDE_DEFAULT_IMAGE=ON.
#
# Hosts that want the bundled vendor firmware image link against
# tmf8829::default_image and bind tmf8829_fw_source_read to
# drv->fw_image_read. Hosts that supply their own image (e.g. from external
# flash, FATFS, or a different firmware revision) ignore this target.

add_library(tmf8829_default_image STATIC
  tmf8829_fw_source.c
  tmf8829_fw_image.c
)

add_library(tmf8829::default_image ALIAS tmf8829_default_image)

target_include_directories(tmf8829_default_image PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(tmf8829_default_image PUBLIC
  tmf8829::tmf8829
)

target_compile_features(tmf8829_default_image PRIVATE c_std_11)
