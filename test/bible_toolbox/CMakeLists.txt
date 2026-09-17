add_executable(BibleToolboxTest
        BibleToolboxTest.cpp
        ${REPO_ROOT}/lib/BibleToolbox/src/BibleToolbox.cpp
)

target_include_directories(BibleToolboxTest PRIVATE
        ${REPO_ROOT}/lib/BibleToolbox/src
        ${REPO_ROOT}/src/util
)

target_link_libraries(BibleToolboxTest PRIVATE
        crosspoint_test_common
        GTest::gtest_main
        sqlite3
)

gtest_discover_tests(BibleToolboxTest)
