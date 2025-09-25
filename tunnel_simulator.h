// tunnel_simulator.h
#pragma once
#include <random>
#include <chrono>
#include <thread>
#include <mutex>
#include <iostream>
#include <string>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class TunnelSimulator {
private:
    int cars;
    double temperature;
    int co;
    int fanSpeed;               // vise nema globalnog speed_fan

    std::mt19937 gen1;
    std::mt19937 gen2;
    std::uniform_int_distribution<int> addDist;
    std::uniform_int_distribution<int> reduceDist;
    std::mutex data_mutex;

    const std::string SERVER_ADDRESS = "tcp://localhost:1883";
    const std::string CLIENT_ID = "TunnelSimulator";
    mqtt::async_client client;

    // Callback klasa
    class FanCallback : public virtual mqtt::callback {
    private:
        TunnelSimulator& simulator;
    public:
        explicit FanCallback(TunnelSimulator& sim) : simulator(sim) {}

        void message_arrived(mqtt::const_message_ptr msg) override {
            std::string payload = msg->to_string();
            try {
                json j = json::parse(payload);
                if (j.contains("fanSpeed")) {
                    int speed = j["fanSpeed"].get<int>();
                    {
                        std::lock_guard<std::mutex> lock(simulator.data_mutex);
                        simulator.fanSpeed = speed;
                    }
                    std::cout << "[TUNNEL] Fan speed set to " << speed << "%" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[TUNNEL] JSON parse error: " << e.what()
                          << " | Payload: " << payload << std::endl;
            }
        }
    } cb;

public:
    TunnelSimulator()
        : cars(15), temperature(40.0), co(400), fanSpeed(0),
          gen1(std::random_device{}()), addDist(0, 5),
          gen2(std::random_device{}()), reduceDist(0, 2),
          client(SERVER_ADDRESS, CLIENT_ID),
          cb(*this) // callback dobija referencu na simulator
    {
        try {
            client.set_callback(cb);
            client.connect()->wait();
            client.subscribe("simulation/fan", 1)->wait();
            std::cout << "[TUNNEL] Subscribed to 'actuators/fan'" << std::endl;
        } catch (const mqtt::exception& exc) {
            std::cerr << "[TUNNEL] MQTT error: " << exc.what() << std::endl;
        }
    }

    // Simulacija
    void run() {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(data_mutex);
                cars =  addDist(gen1) - reduceDist(gen2);
                
                if (fanSpeed > 0) {
                    temperature +=  cars * 0.7 - 5 * fanSpeed/100.0;
                    std::cout<<"Temperatura se povecala za  "<<cars * 0.7<<std::endl;
                    std::cout<<"Temperatura se smanjila za  "<<5 * fanSpeed/100.0<<std::endl;
                    co += cars * 40 - 80 * fanSpeed / 100;
                    std::cout<<"CO se smanjio za  "<<80 * fanSpeed/100<<std::endl;
                } else {
                    temperature += cars * 0.7;
                    co +=  cars * 40;
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }

    // Metode za pristup vrednostima
    int getCars() {
        std::lock_guard<std::mutex> lock(data_mutex);
        return cars;
    }

    double getTemperature() {
        std::lock_guard<std::mutex> lock(data_mutex);
        return temperature;
    }

    int getco() {
        std::lock_guard<std::mutex> lock(data_mutex);
        return co;
    }

    int getFanSpeed() {
        std::lock_guard<std::mutex> lock(data_mutex);
        return fanSpeed;
    }
};
