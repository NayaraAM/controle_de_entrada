cmake_minimum_required(VERSION 3.16)
project(EstacionamentoATR LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Encontra a Paho que está instalada no sistema (método estável)
find_package(PahoMqttCpp REQUIRED)

# 2. Adiciona a CPR a partir da pasta local que acabamos de baixar (método contido)
set(CPR_BUILD_TESTS OFF) # Desliga a compilação dos testes da CPR
add_subdirectory(cpr)

# 3. Adiciona seus três programas
add_executable(DataProcessor src/DataProcessor.cpp)
add_executable(Guiche_Entrada src/Guiche_Entrada.cpp)
add_executable(Guiche_Saida src/Guiche_Saida.cpp)

# 4. Conecta (linka) as bibliotecas
target_link_libraries(DataProcessor PRIVATE PahoMqttCpp::paho-mqttpp3 cpr::cpr)
target_link_libraries(Guiche_Entrada PRIVATE PahoMqttCpp::paho-mqttpp3)
target_link_libraries(Guiche_Saida PRIVATE PahoMqttCpp::paho-mqttpp3)