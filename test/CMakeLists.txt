CMAKE_MINIMUM_REQUIRED(VERSION 3.16)

PROJECT(GrootTest LANGUAGES CXX)

find_package(Qt5Test REQUIRED)
find_package(Qt5Gui REQUIRED)

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_INCLUDE_CURRENT_DIR ON)
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
ENABLE_TESTING()

function(CompileTest name)
    add_executable(${name} ${name}.cpp groot_test_base.cpp ${RESOURCE_FILES} )
    target_link_libraries(${name} PRIVATE Qt5::Gui Qt5::Test behavior_tree_editor)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

set(RESOURCE_FILES
    ../QtNodeEditor/resources/resources.qrc
    ../bt_editor/resources/icons.qrc
    ../bt_editor/resources/style.qrc
    ../test_data/test_files.qrc
    )

option(GROOT_BUILD_LEGACY_TESTS "Build the original v3 fixture tests" OFF)
if(GROOT_BUILD_LEGACY_TESTS)
    CompileTest(editor_test)
    CompileTest(replay_test)
endif()

add_executable(v4_compat_test v4_compat_test.cpp ${RESOURCE_FILES})
target_link_libraries(v4_compat_test PRIVATE Qt5::Gui Qt5::Test behavior_tree_editor)
add_test(NAME v4_compat_test COMMAND v4_compat_test modelLessRoundTrip subtreeRoundTrip runtimeTreeAndLogReplay)
set_tests_properties(v4_compat_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=minimal" TIMEOUT 20)
