# Tree-sitter C runtime - vendored static library
# Tree-sitter C calisma zamani - dahil edilmis statik kutuphane

add_library(tree-sitter STATIC
    src/lib.c
)

# Public include path for tree_sitter/api.h
# tree_sitter/api.h icin acik dahil etme yolu
target_include_directories(tree-sitter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Internal include path for source files
# Kaynak dosyalar icin dahili dahil etme yolu
target_include_directories(tree-sitter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# C11 standard required by tree-sitter
# Tree-sitter tarafindan gereklenen C11 standardi
set_target_properties(tree-sitter PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Suppress warnings from vendored code
# Dahil edilmis koddan gelen uyarilari bastir
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(tree-sitter PRIVATE
        -w
    )
elseif(MSVC)
    target_compile_options(tree-sitter PRIVATE
        /w
    )
endif()
