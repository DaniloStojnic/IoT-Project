#include <iostream>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Globalne promenljive
double temperature = 0.0;
int co = 0;
int fanSpeed = 0;
bool buzzerOn = false;
std::mutex data_mutex;

// Callback za sve MQTT poruke
class AppCallback : public virtual mqtt::callback {
public:
    void message_arrived(mqtt::const_message_ptr msg) override {
        std::string topic = msg->get_topic();
        std::string payload = msg->to_string();

        try {
            json j = json::parse(payload);
            std::lock_guard<std::mutex> lock(data_mutex);

            if (topic == "sensors/temperature" && j.contains("value")) {
                temperature = j["value"].get<double>();
            }
            else if (topic == "sensors/co" && j.contains("value")) {
                co = j["value"].get<int>();
            }
            else if (topic == "actuators/fan" && j.contains("fanSpeed")) {
                fanSpeed = j["fanSpeed"].get<int>();
            }
            else if (topic == "actuators/buzzer" && j.contains("buzzer")) {
                buzzerOn = j["buzzer"].get<bool>();
            }

        } catch (const std::exception& e) {
            std::cerr << "[APP] JSON parse error: " << e.what()
                      << " | Payload: " << payload << std::endl;
        }
    }
};

int main() {
    const std::string SERVER_ADDRESS("tcp://localhost:1883");
    const std::string CLIENT_ID("ApplicationMonitor");

    mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);
    AppCallback cb;
    client.set_callback(cb);

    try {
        client.connect()->wait();

        // Subscribe na sve teme
        client.subscribe("sensors/temperature", 1)->wait();
        client.subscribe("sensors/co", 1)->wait();
        client.subscribe("actuators/fan", 1)->wait();
        client.subscribe("actuators/buzzer", 1)->wait();

        while (true) {
            // Očisti terminal
            std::cout << "\033[2J\033[1;1H";

            {
                std::lock_guard<std::mutex> lock(data_mutex);
                std::cout << "==============================" << std::endl;
                std::cout << " TEMPERATURE & co MONITOR " << std::endl;
                std::cout << "==============================" << std::endl;
                std::cout << "Temperature: " << temperature << " °C" << std::endl;
                std::cout << "co Level: " << co << " ppm" << std::endl;
                std::cout << "Fan Speed: " << fanSpeed << " %" << std::endl;
                std::cout << "Buzzer: " << (buzzerOn ? "ON" : "OFF") << std::endl;
                std::cout << "==============================" << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        client.disconnect()->wait();
    } catch (const mqtt::exception& exc) {
        std::cerr << "Error: " << exc.what() << std::endl;
        return 1;
    }

    return 0;
}
