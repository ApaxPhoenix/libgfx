# Compiles each entry point of a Slang file to SPIR-V with slangc and embeds it as a header of
# uint32_t words, <binary>/generated/shaders/<stage>.h, so nothing is read from disk at runtime.
#
#   gfx_shaders(<target> <file.slang> <stage>...)

function(gfx_shaders TARGET SOURCE)
    set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated")
    foreach(STAGE IN LISTS ARGN)
        set(HEADER "${OUTPUT}/shaders/${STAGE}.h")
        add_custom_command(
                OUTPUT "${HEADER}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${OUTPUT}/shaders"
                COMMAND "${SLANGC}" "${SOURCE}"
                        -entry "${STAGE}" -stage "${STAGE}" -target spirv -profile spirv_1_3
                        -matrix-layout-column-major -source-embed-style u32
                        -source-embed-name "${STAGE}" -o "${HEADER}"
                DEPENDS "${SOURCE}"
                COMMENT "Compiling shader ${STAGE}"
                VERBATIM
        )
        target_sources(${TARGET} PRIVATE "${HEADER}")
    endforeach()
    target_include_directories(${TARGET} PRIVATE "${OUTPUT}")
endfunction()
