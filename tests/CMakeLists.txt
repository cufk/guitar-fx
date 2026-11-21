add_executable(gfx-tests tests.cpp)
target_link_libraries(gfx-tests PRIVATE gfx_core)
add_test(NAME gfx-tests COMMAND gfx-tests)
