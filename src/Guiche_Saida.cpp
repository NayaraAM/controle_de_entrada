// GuicheSaida.cpp
#include <iostream>
#include <string>
#include <chrono>    // Para std::chrono::system_clock, std::chrono::seconds, std::this_thread::sleep_for
#include <ctime>     // Para std::time_t, std::gmtime, std::localtime
#include <iomanip>   // Para std::put_time
#include <random>    // Para std::random_device, std::mt19937, std::uniform_int_distribution
#include <sstream>   // Para std::stringstream
#include <thread>    // Para std::this_thread::sleep_for (se fosse ter delay, mas nao eh o caso aqui)

// --- Incluir a biblioteca Paho MQTT C++ ---
#include "mqtt/async_client.h"

// --- Incluir a biblioteca nlohmann/json ---
#include "nlohmann/json.hpp" 

// --- Definição do cliente MQTT e tópicos ---
const std::string SERVER_ADDRESS("tcp://localhost:1883");
const std::string CLIENT_ID_GUICHE_SAIDA = "GuicheSaida_Cliente"; // ID único para a conexão MQTT
const std::string MACHINE_ID = "estacionamento_tempo_real"; // ID da "máquina" (estacionamento)
const std::string SENSOR_ID_SAIDA = "guiche_saida_1"; // ID específico deste sensor/guichê

// Função para gerar um timestamp no formato ISO 8601 UTC
std::string getCurrentISO8601Time() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_gmt = std::gmtime(&now_c); // Tempo GMT/UTC

    std::stringstream ss;
    ss << std::put_time(tm_gmt, "%Y-%m-%dT%H:%M:%SZ"); // Formato ISO 8601 UTC
    return ss.str();
}

int main(int argc, char* argv[]) {
    // O Guichê de Saída precisa saber qual placa ele vai "sair".
    // Para a demo, você passará a placa como argumento de linha de comando.
    // Exemplo de execução: ./GuicheSaida ABC1D23
    if (argc < 2) {
        std::cerr << "[" << SENSOR_ID_SAIDA << "]: ERRO! Uso: " << argv[0] << " <PLACA_DO_CARRO>" << std::endl;
        std::cerr << "         Exemplo: " << argv[0] << " ABC1D23" << std::endl;
        return 1;
    }
    std::string placa_saida = argv[1]; // Pega a placa do primeiro argumento

    mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID_GUICHE_SAIDA);
    mqtt::connect_options connOpts;
    connOpts.set_clean_session(true);

    try {
        std::cout << "[" << SENSOR_ID_SAIDA << "]: Conectando ao broker MQTT em " << SERVER_ADDRESS << "..." << std::endl;
        mqtt::token_ptr conntok = client.connect(connOpts);
        conntok->wait();
        std::cout << "[" << SENSOR_ID_SAIDA << "]: Conectado." << std::endl;

        // --- Geração de dados para a mensagem de saída ---
        std::string timestamp_gerado = getCurrentISO8601Time();
        double valor_pago_simulado = 15.00; // Valor fixo simulado de pagamento

        // --- Construção da mensagem JSON de saída ---
        nlohmann::json msg_payload;
        msg_payload["timestamp"] = timestamp_gerado;
        msg_payload["value"] = 0.0; // Adicionado campo 'value' conforme requisito (0.0 para saída)
        msg_payload["tipo_evento"] = "saida"; 
        msg_payload["placa"] = placa_saida;
        msg_payload["id_guiche"] = SENSOR_ID_SAIDA;
        msg_payload["valor_pago"] = valor_pago_simulado; // Adiciona o valor pago simulado

        std::string json_str = msg_payload.dump();
        
        // Define o tópico de publicação para saída
        // Formato: /sensors/<id_da_maquina>/<id_do_sensor>
        std::string topic_publicacao = "/sensors/" + MACHINE_ID + "/" + SENSOR_ID_SAIDA;
        
        mqtt::message_ptr pubmsg = mqtt::make_message(topic_publicacao, json_str);
        pubmsg->set_qos(1); // QoS 1 para garantir entrega
        
        std::cout << "[" << SENSOR_ID_SAIDA << "]: Publicando evento de saida para placa [" << placa_saida << "]" << std::endl;
        std::cout << "Topico: " << topic_publicacao << std::endl;
        std::cout << "Payload: " << json_str << std::endl;

        client.publish(pubmsg)->wait_for(std::chrono::seconds(10)); // Publica e espera por confirmação
        std::cout << "[" << SENSOR_ID_SAIDA << "]: Evento de saida publicado com sucesso." << std::endl;

        std::cout << "[" << SENSOR_ID_SAIDA << "]: Desconectando do broker..." << std::endl;
        client.disconnect()->wait();
        std::cout << "[" << SENSOR_ID_SAIDA << "]: Desconectado." << std::endl;

    } catch (const mqtt::exception& exc) {
        std::cerr << "Erro MQTT (" << SENSOR_ID_SAIDA << "): " << exc.what() << std::endl;
        return 1;
    } catch (const std::exception& exc) {
        std::cerr << "Erro geral (" << SENSOR_ID_SAIDA << "): " << exc.what() << std::endl;
        return 1;
    }

    return 0;
}