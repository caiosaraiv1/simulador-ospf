add_executable(test_dijkstra
    test_dijkstra.cpp
    ../src/roteador.cpp
)

target_include_directories(test_dijkstra PRIVATE ../include)
target_link_libraries(test_dijkstra PRIVATE nlohmann_json::nlohmann_json)
