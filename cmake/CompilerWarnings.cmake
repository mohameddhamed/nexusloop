# Compiler Warnings Configuration for NexusLoop
function(nexus_set_compiler_warnings target_name)
    target_compile_options(${target_name} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
    )
endfunction()
