file(GLOB_RECURSE FMLUME_TEST_SOURCES CONFIGURE_DEPENDS *.cpp *.h)
add_executable(FMLUMETests ${FMLUME_TEST_SOURCES})
target_include_directories(FMLUMETests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(FMLUMETests PRIVATE FMLUMECore)
target_compile_definitions(FMLUMETests PRIVATE
    FMLUME_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# One CTest entry per test group (file-level tag) keeps failures readable.
foreach(group dsp unit integration stress)
    add_test(NAME fmlume_${group} COMMAND FMLUMETests ${group})
endforeach()
