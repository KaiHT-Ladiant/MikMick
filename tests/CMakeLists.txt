find_package(Qt6 REQUIRED COMPONENTS Test)

function(mikmick_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
    target_compile_options(${name} PRIVATE ${MIKMICK_WARNINGS})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

mikmick_test(tst_core mm_core)
mikmick_test(tst_capture mm_tools)
mikmick_test(tst_options mm_options)
mikmick_test(tst_editor mm_editor)

file(GLOB _controller ${PROJECT_SOURCE_DIR}/src/app/Controller.*)
add_executable(mikmick-screenshots make_screenshots.cpp ${_controller})
target_link_libraries(mikmick-screenshots PRIVATE Qt6::Test mm_editor mm_tools mm_options mm_capture mm_ui mm_core)
target_compile_options(mikmick-screenshots PRIVATE ${MIKMICK_WARNINGS})
