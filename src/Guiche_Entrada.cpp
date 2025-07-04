#include "mqtt/async_client.h"
#include "nlohmann/json.hpp"

#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <thread>
#include <sstream>

// Definição do cliente MQTT
const std::string SERVER_ADDRESS("tcp://localhost:1883");
const std::string CLIENT_ID = "ClientEntrada_1";
const std::string GUICHE_ID = "GuicheEntrada_1";
const std::string SENSOR_ID = "SensorEntrada_1";

// Função para gerar uma placa aleatória no padrão Mercosul
std::string gerarPlacaMercosul() {
    const std::string letras = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string numeros = "0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist_letras(0, letras.size() - 1);
    std::uniform_int_distribution<> dist_numeros(0, numeros.size() - 1);

    std::string placa;
    for (int i = 0; i < 3; ++i) placa += letras[dist_letras(gen)];
    placa += numeros[dist_numeros(gen)];
    placa += letras[dist_letras(gen)];
    for (int i = 0; i < 2; ++i) placa += numeros[dist_numeros(gen)];

    return placa;
}

// Função para gerar um timestamp no formato ISO 8601 UTC (CORRIGIDO)
std::string gerarTimestampISO8601() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    
    std::tm timeinfo;
    gmtime_r(&now_c, &timeinfo); // Usar gmtime_r para ser thread-safe e UTC

    std::stringstream ss;
    // Formato correto: ANO-MES-DIA'T'HORA:MINUTO:SEGUNDO'Z'
    ss << std::put_time(&timeinfo, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

int main() {
    mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);
    mqtt::connect_options connOpts;
    connOpts.set_clean_session(true);

    try {
        std::cout << GUICHE_ID << ": Conectando ao broker MQTT em " << SERVER_ADDRESS << "..." << std::endl;
        client.connect(connOpts)->wait();
        std::cout << GUICHE_ID << ": Conectado." << std::endl;

        // --- Geração de dados para a mensagem ---
        std::string placa_gerada = gerarPlacaMercosul();
        std::string timestamp_gerado = gerarTimestampISO8601(); // Chamando a função correta

        // --- Construção da mensagem JSON ---
        nlohmann::json msg_payload;
        msg_payload["timestamp"] = timestamp_gerado;
        msg_payload["tipo_evento"] = "entrada";
        msg_payload["placa"] = placa_gerada;
        msg_payload["id_guiche"] = SENSOR_ID;

        std::string json_str = msg_payload.dump();

        // --- Definição do Tópico de Publicação ---
        std::string topic_publicacao = "/sensors/" + GUICHE_ID + "/" + SENSOR_ID;
        
        mqtt::message_ptr pubmsg = mqtt::make_message(topic_publicacao, json_str);
        pubmsg->set_qos(1);

        std::cout << GUICHE_ID << ": Publicando evento de entrada para placa [" << placa_gerada << "]" << std::endl;
        std::cout << "Topico: " << topic_publicacao << std::endl;
        std::cout << "Payload: " << json_str << std::endl;

        client.publish(pubmsg)->wait_for(std::chrono::seconds(10));
        std::cout << GUICHE_ID << ": Evento de entrada publicado com sucesso." << std::endl;

        // --- Publica a Mensagem Inicial do DataCollector ---
        nlohmann::json initial_monitor_msg;
        initial_monitor_msg["machine_id"] = GUICHE_ID;
        initial_monitor_msg["sensors"] = nlohmann::json::array();
        
        nlohmann::json sensor_info;
        sensor_info["sensor_id"] = SENSOR_ID;
        sensor_info["data_type"] = "json_evento_carro";
        sensor_info["data_interval"] = "simulado";
        initial_monitor_msg["sensors"].push_back(sensor_info);
        
        // Adiciona informações do guichê de saída também
        nlohmann::json sensor_info_saida;
        sensor_info_saida["sensor_id"] = "guiche_saida_1";
        sensor_info_saida["data_type"] = "json_evento_carro";
        sensor_info_saida["data_interval"] = "simulado";
        initial_monitor_msg["sensors"].push_back(sensor_info_saida);

        std::string initial_monitor_json = initial_monitor_msg.dump();
        mqtt::message_ptr monitor_msg = mqtt::make_message("/sensor_monitors", initial_monitor_json);
        monitor_msg->set_qos(1);

        std::cout << GUICHE_ID << ": Publicando mensagem inicial no topico /sensor_monitors..." << std::endl;
        std::cout << "Payload: " << initial_monitor_json << std::endl;
        client.publish(monitor_msg)->wait_for(std::chrono::seconds(10));
        std::cout << GUICHE_ID << ": Mensagem inicial publicada." << std::endl;

        // Desconectando
        std::cout << GUICHE_ID << ": Desconectando do broker..." << std::endl;
        client.disconnect()->wait();
        std::cout << GUICHE_ID << ": Desconectado." << std::endl;

    } catch (const mqtt::exception& exc) {
        std::cerr << "Erro MQTT (" << GUICHE_ID << "): " << exc.what() << std::endl;
        return 1;
    } catch (const std::exception& exc) {
        std::cerr << "Erro geral (" << GUICHE_ID << "): " << exc.what() << std::endl;
        return 1;
    }

    return 0;
}