aux_source_directory(./src SRC)

set(LIBRARY_OUTPUT_PATH ${LIB_PATH})

add_library(${ENGINE_NAME} SHARED ${SRC})


target_include_directories(${ENGINE_NAME} PUBLIC ${HEADER_PATH})
