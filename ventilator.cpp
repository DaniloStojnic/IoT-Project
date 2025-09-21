#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>

using json = nlohmann::json;

const std::string SERVER_ADDRESS("tcp://localhost:1883");
const std::string CLIENT_ID("FanSimulator");

// SSDP multicast adresa i port
const char* SSDP_ADDR = "239.255.255.250";
const int SSDP_PORT = 1900;

// Callback za MQTT
class FanCallback : public virtual mqtt::callback {
public:
    void message_arrived(mqtt::const_message_ptr msg) override {
        std::string payload = msg->to_string();
        try {
            json j = json::parse(payload);
            int speed = 0;

            if (j.contains("fanSpeed")) {
                speed = j["fanSpeed"].get<int>();
            }

            if (speed > 0){
                std::cout << "[VENTILATOR] Radi sa " << speed << "% snage." << std::endl;
            } else {
                std::cout << "[VENTILATOR] Ventilator je isključen." << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "[VENTILATOR] JSON parse error: " << e.what()
                      << " | Payload: " << payload << std::endl;
        }
    }
};

// Thread koji osluškuje SSDP M-SEARCH i proverava timeout
void ssdpMonitorThread() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket error");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SSDP_PORT);
    inet_pton(AF_INET, SSDP_ADDR, &addr.sin_addr);

    std::string msearchMsg =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 3\r\n"
        "ST: urn:schemas-upnp-org:device:Controller:1\r\n\r\n";

    sockaddr_in recvAddr{};
    socklen_t addrLen = sizeof(recvAddr);
    char buffer[1024];

    auto lastSeen = std::chrono::steady_clock::now();

    while (true) {
        // šalje M-SEARCH poruku
        sendto(sock, msearchMsg.c_str(), msearchMsg.size(), 0, (struct sockaddr*)&addr, sizeof(addr));

        // prima eventualni odgovor
        int n = recvfrom(sock, buffer, sizeof(buffer)-1, MSG_DONTWAIT, (struct sockaddr*)&recvAddr, &addrLen);
        if (n > 0) {
            buffer[n] = '\0';
            std::string resp(buffer);
            if (resp.find("uuid:microcontroller-001") != std::string::npos) {
                lastSeen = std::chrono::steady_clock::now();
            }
        }

        // proverava timeout
        if (std::chrono::steady_clock::now() - lastSeen > std::chrono::seconds(60)) {
            std::cerr << "[VENTILATOR] Microcontroller not found! Shutting down..." << std::endl;
            exit(0);
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

int main() {
    std::cout<<"VENTILATOR"<<std::endl;
    mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);
    FanCallback cb;
    client.set_callback(cb);

    // Pokretanje SSDP monitora
    std::thread ssdpThread(ssdpMonitorThread);

    try {
        client.connect()->wait();
        client.subscribe("actuators/fan", 1)->wait();
        std::cout<<"==========================================="<<std::endl;
        std::cout << "[VENTILATOR] Fan simulator subscribed to 'actuators/fan'" << std::endl;

        // Glavna petlja MQTT
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        client.disconnect()->wait();
    } catch (const mqtt::exception& exc) {
        std::cerr << "Error: " << exc.what() << std::endl;
        return 1;
    }

    ssdpThread.join();
    return 0;
}
