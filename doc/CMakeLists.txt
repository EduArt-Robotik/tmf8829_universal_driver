#=====================================
# Generate Doxygen documentation
#=====================================

find_package(Doxygen REQUIRED)
find_package(Doxygen OPTIONAL_COMPONENTS dot)

set(DOXYGEN_CONFIG_IN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doxygen.config.in")
set(DOXYGEN_CONFIG_FILE    "${CMAKE_CURRENT_BINARY_DIR}/doxygen.config")

# Collect image files so they can be served as HTML_EXTRA_FILES.
file(GLOB DOXYGEN_IMAGES "${CMAKE_CURRENT_SOURCE_DIR}/images/*")

set(DOXYGEN_IMAGES_REL "")
foreach(img ${DOXYGEN_IMAGES})
  file(RELATIVE_PATH rel_path "${CMAKE_CURRENT_SOURCE_DIR}" "${img}")
  list(APPEND DOXYGEN_IMAGES_REL "${rel_path}")
endforeach()

string(JOIN " " DOXYGEN_IMAGES_LIST ${DOXYGEN_IMAGES_REL})

#=====================================
# Fetch doxygen-awesome-css
#=====================================
include(FetchContent)
FetchContent_Declare(
  doxygen-awesome-css
  URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/heads/main.zip
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
)
FetchContent_MakeAvailable(doxygen-awesome-css)

FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)

#=====================================
# Generate config and build docs
#=====================================
configure_file(${DOXYGEN_CONFIG_IN_FILE} ${DOXYGEN_CONFIG_FILE} @ONLY)

add_custom_target(
  doc ALL
  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG_FILE}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Generating TMF8829 documentation..."
  VERBATIM
)
