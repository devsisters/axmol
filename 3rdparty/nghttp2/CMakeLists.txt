set(lib_name nghttp2)
set(target_name ${lib_name})

project(${lib_name})

_1kfetch_dist(nghttp2)

if(WINDOWS)
  add_library(${target_name} SHARED IMPORTED GLOBAL)
else()
  add_library(${target_name} STATIC IMPORTED GLOBAL)
endif()

set_target_properties(${target_name} PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${nghttp2_INC_DIR}"
)

if(WINDOWS)
  set_target_properties(${target_name} PROPERTIES
    IMPORTED_LOCATION "${nghttp2_LIB_DIR}/lib${lib_name}.dll"
    IMPORTED_IMPLIB "${nghttp2_LIB_DIR}/lib${lib_name}_imp.lib"
  )
elseif(APPLE)
  set_target_properties(${target_name} PROPERTIES
    IMPORTED_LOCATION "${nghttp2_LIB_DIR}/${lib_name}.xcframework"
  )
else()
  set_target_properties(${target_name} PROPERTIES
    IMPORTED_LOCATION "${nghttp2_LIB_DIR}/lib${lib_name}.a"
  )
endif()