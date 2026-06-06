find_package(GTest REQUIRED)

add_executable(test_dijkstra
    test_dijkstra.cpp
    ../src/roteador.cpp
)
target_include_directories(test_dijkstra PRIVATE ../include)
target_link_libraries(test_dijkstra PRIVATE
    nlohmann_json::nlohmann_json
    GTest::gtest
)

add_executable(test_threads
    test_threads.cpp
    ../src/roteador.cpp
)
target_include_directories(test_threads PRIVATE ../include)
target_link_libraries(test_threads PRIVATE
    nlohmann_json::nlohmann_json
    GTest::gtest
)
