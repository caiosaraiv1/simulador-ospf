find_package(GTest REQUIRED)

add_executable(test_dijkstra
    test_dijkstra.cpp
    ../src/roteador.cpp
    ../src/simulador.cpp
      ../src/logger.cpp
)
target_include_directories(test_dijkstra PRIVATE ../include)
target_link_libraries(test_dijkstra PRIVATE
    nlohmann_json::nlohmann_json
    GTest::gtest
)

add_executable(test_threads
    test_threads.cpp
    ../src/roteador.cpp
    ../src/simulador.cpp
    ../src/logger.cpp
)
target_include_directories(test_threads PRIVATE ../include)
target_link_libraries(test_threads PRIVATE
    nlohmann_json::nlohmann_json
    GTest::gtest
)

add_executable(test_ciclo_vida
    test_ciclo_vida.cpp
    ../src/roteador.cpp
    ../src/simulador.cpp
      ../src/logger.cpp
)
target_include_directories(test_ciclo_vida PRIVATE ../include)
target_link_libraries(test_ciclo_vida PRIVATE
    nlohmann_json::nlohmann_json
    GTest::gtest
)

add_executable(test_ospf_convergencia
   test_ospf_convergencia.cpp
    ../src/roteador.cpp
    ../src/simulador.cpp
    ../src/logger.cpp
)
target_include_directories(test_ospf_convergencia PRIVATE ../include)
target_link_libraries(test_ospf_convergencia PRIVATE
    nlohmann_json::nlohmann_json
    GTest::gtest
)

add_executable(test_ressureicao
   test_ressureicao.cpp
   ../src/roteador.cpp
   ../src/simulador.cpp
   ../src/logger.cpp
)
target_include_directories(test_ressureicao PRIVATE ../include)
target_link_libraries(test_ressureicao PRIVATE
    nlohmann_json::nlohmann_json
    GTest::gtest
)

add_executable(test_simulacao
    test_simulacao.cpp
    ../src/roteador.cpp
    ../src/simulador.cpp
    ../src/logger.cpp
)
target_include_directories(test_simulacao PRIVATE ../include)
target_link_libraries(test_simulacao PRIVATE
    nlohmann_json::nlohmann_json
    GTest::gtest
)
