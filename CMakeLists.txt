cmake_minimum_required(VERSION 3.20)

# Define o nome do projeto e a linguagem (C++)
project(SimuladorOSPF VERSION 1.0 LANGUAGES CXX)

# Padrão C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Gera um arquivo que ajuda o seu editor (VSCode/Neovim) a aplicar o Clang-Tidy depois
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

# 1. Encontrar as dependências via Conan
find_package(nlohmann_json REQUIRED)
# find_package(GTest REQUIRED) # Deixado comentado até configurarmos a pasta tests/

# 2. Configurar Sanitizers (O "Chaos Engineering" da Memória)
# Atenção: Você não pode usar AddressSanitizer e ThreadSanitizer ao mesmo tempo. 
add_compile_options(-Wall -Wextra -Wpedantic -fsanitize=address -g)
add_link_options(-fsanitize=address)

# 3. Mapear os diretórios e arquivos
set(INCLUDES "include")
set(SOURCES 
    src/main.cpp
    src/roteador.cpp
    src/simulador.cpp
)

# 4. Criar o Executável Principal
add_executable(simulador_ospf ${SOURCES})
target_include_directories(simulador_ospf PRIVATE ${INCLUDES})

# 5. Linkar as bibliotecas ao nosso executável
target_link_libraries(simulador_ospf PRIVATE nlohmann_json::nlohmann_json)

# 6. Testes (Opcional por enquanto, mas já deixamos o gancho estrutural)
# add_subdirectory(tests)
