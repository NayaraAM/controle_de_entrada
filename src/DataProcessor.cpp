#include <iostream>
#include <string>
#include <map>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <mutex>
#include <vector>

#include "mqtt/async_client.h"
#include "nlohmann/json.hpp"
#include "cpr/cpr.h" 

// ===================================================================
//      CONFIGURAÇÕES InfluxDB e MQTT
// ===================================================================
#define QOS 1
const std::string BROKER_ADDRESS("tcp://localhost:1883");
const std::string CLIENT_ID_DATAPROCESSOR = "DataProcessor_Cliente";
const std::string SENSORS_TOPIC = "/sensors/#";

const std::string INFLUX_URL = "http://localhost:8086";
const std::string INFLUX_API_TOKEN = "f1hzsRrA00woSrboUKju3uQ6bsvRt6rY4eM6wbFVgY-oGKjc9pxaJLTfgyo68_Pn88rSWl5s-Ed9YxqnKJBbCw==";
const std::string INFLUX_ORG = "ATR";
const std::string INFLUX_BUCKET = "Estacionamento Tempo Real";
// ===================================================================

struct CarEntry {
    std::string placa;
    std::chrono::system_clock::time_point timestamp_entrada_tp;
};

std::map<std::string, CarEntry> carros_no_estacionamento;
std::mutex carros_mutex;

void writeToInfluxDB(const std::string& line_protocol) {
    std::string full_url = INFLUX_URL + "/api/v2/write?org=" + INFLUX_ORG + "&bucket=" + INFLUX_BUCKET + "&precision=ns";
    cpr::Response r = cpr::Post(cpr::Url{full_url},
                                cpr::Header{{"Authorization", "Token " + INFLUX_API_TOKEN}},
                                cpr::Body{line_protocol},
                                cpr::Timeout{5000});

    if (r.status_code >= 400) {
        std::cerr << "[InfluxDB]: ERRO ao escrever! Status: " << r.status_code << std::endl;
        std::cerr << "  -> Mensagem: " << r.text << std::endl;
    } else {
        std::cout << "[InfluxDB]: Dado escrito com sucesso: " << line_protocol << std::endl;
    }
}

std::chrono::system_clock::time_point parseISO8601(const std::string& timestamp_str) {
    std::tm t{};
    std::istringstream ss(timestamp_str);
    ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }
    // timegm é a versão de mktime que lida corretamente com UTC.
    // É uma extensão GNU, mas deve funcionar no seu ambiente Ubuntu.
    return std::chrono::system_clock::from_time_t(timegm(&t));
}

void processarMensagemAsync(mqtt::const_message_ptr msg_ptr) {
    try {
        std::string topic = msg_ptr->get_topic();
        nlohmann::json payload = nlohmann::json::parse(msg_ptr->to_string());

        std::cout << "[DataProcessor - Thread " << std::this_thread::get_id() << "]: Recebida mensagem em topico '" << topic << "'" << std::endl;

        if (topic == "/sensor_monitors") {
            std::cout << "[DataProcessor]: Mensagem de monitoramento recebida: " << payload.dump(2) << std::endl;
            return;
        }

        if (payload.count("tipo_evento") && payload.count("placa")) {
            std::string tipo_evento = payload["tipo_evento"];
            std::string placa = payload["placa"];
            std::string timestamp_str = payload["timestamp"];
            
            // ===================================================================
            //      MUDANÇA PARA O TESTE: Usar sempre a hora atual
            // ===================================================================
            auto timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                    std::chrono::system_clock::now().time_since_epoch()
                                ).count();
            // ===================================================================
            
            std::lock_guard<std::mutex> lock(carros_mutex);

            if (tipo_evento == "entrada") {
                carros_no_estacionamento[placa] = {placa, parseISO8601(timestamp_str)};
                std::cout << "[DataProcessor]: Carro " << placa << " entrou. Registrado." << std::endl;
                
                std::string data_point = "eventos,tipo=entrada,placa=" + placa + " value=1 " + std::to_string(timestamp_ns);
                writeToInfluxDB(data_point);

            } else if (tipo_evento == "saida") {
                if (carros_no_estacionamento.count(placa)) {
                    auto timestamp_entrada_tp = carros_no_estacionamento[placa].timestamp_entrada_tp;
                    auto timestamp_saida_tp = parseISO8601(timestamp_str);
                    std::chrono::duration<double> duracao = timestamp_saida_tp - timestamp_entrada_tp;
                    long long minutos = static_cast<long long>(duracao.count()) / 60;

                    std::cout << "[DataProcessor]: Carro " << placa << " saiu. Tempo de permanencia: " << minutos << " min." << std::endl;
                    carros_no_estacionamento.erase(placa);

                    std::string data_point_saida = "eventos,tipo=saida,placa=" + placa + " value=1 " + std::to_string(timestamp_ns);
                    std::string data_point_tempo = "permanencia,placa=" + placa + " minutos=" + std::to_string(minutos) + " " + std::to_string(timestamp_ns);
                    writeToInfluxDB(data_point_saida);
                    writeToInfluxDB(data_point_tempo);

                } else {
                    std::cout << "[DataProcessor]: ERRO! Carro " << placa << " tentou sair mas nao ha registro de entrada." << std::endl;
                    std::string data_point_alarme = "alarmes,tipo=saida_sem_entrada,placa=" + placa + " value=1 " + std::to_string(timestamp_ns);
                    writeToInfluxDB(data_point_alarme);
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[DataProcessor - Thread]: Erro: " << e.what() << std::endl;
    }
}

class callback : public virtual mqtt::callback {
public:
    void connection_lost(const std::string& cause) override {
        std::cerr << "[DataProcessor]: Conexao perdida: " << cause << std::endl;
    }

    void message_arrived(mqtt::const_message_ptr msg) override {
        std::thread(processarMensagemAsync, msg).detach();
    }
};

int main(int argc, char* argv[]) {
    mqtt::async_client client(BROKER_ADDRESS, CLIENT_ID_DATAPROCESSOR);
    callback cb;
    client.set_callback(cb);

    mqtt::connect_options connOpts;
    connOpts.set_clean_session(true);

    try {
        std::cout << "[" << CLIENT_ID_DATAPROCESSOR << "]: Conectando ao broker MQTT em " << BROKER_ADDRESS << "..." << std::endl;
        client.connect(connOpts)->wait();
        std::cout << "[" << CLIENT_ID_DATAPROCESSOR << "]: Conectado." << std::endl;

        std::cout << "[" << CLIENT_ID_DATAPROCESSOR << "]: Assinando topico geral: " << SENSORS_TOPIC << std::endl;
        client.subscribe(SENSORS_TOPIC, QOS)->wait();

        std::cout << "[" << CLIENT_ID_DATAPROCESSOR << "]: Pronto para receber mensagens. Pressione 'Enter' para sair." << std::endl;
        std::cin.get();

        std::cout << "[" << CLIENT_ID_DATAPROCESSOR << "]: Desconectando..." << std::endl;
        client.disconnect()->wait();
        std::cout << "[" << CLIENT_ID_DATAPROCESSOR << "]: Desconectado." << std::endl;

    } catch (const mqtt::exception& exc) {
        std::cerr << "Erro MQTT (" << CLIENT_ID_DATAPROCESSOR << "): " << exc.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}