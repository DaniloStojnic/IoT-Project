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
#include <unistd.h>

using json = nlohmann::json;

std::string SERVER_ADDRESS; // dobija se iz SSDP LOCATION
const std::string CLIENT_ID("FanSimulator");
std::mutex serverMutex;

// SSDP multicast adresa i port
const char* SSDP_ADDR = "239.255.255.250";
const int SSDP_PORT = 1900;

// Callback za MQTT
class FanCallback : public virtual mqtt::callback {
    mqtt::async_client* client;
public:
    FanCallback(mqtt::async_client* c) : client(c) {}

    void message_arrived(mqtt::const_message_ptr msg) override {
        std::string payload = msg->to_string();
        try {
            json j = json::parse(payload);
            int speed = 0;

            if (j.contains("fanSpeed")) {
                speed = j["fanSpeed"].get<int>();
            }

            if (speed > 0)
                std::cout << "[VENTILATOR] Radi sa " << speed << "% snage." << std::endl;
            else
                std::cout << "[VENTILATOR] Ventilator je isključen." << std::endl;

            // Publikuj status nazad
            json statusMsg = { {"fanSpeed", speed} };
            client->publish("simulation/fan", statusMsg.dump(), 1, false);

        } catch (const std::exception& e) {
            std::cerr << "[VENTILATOR] JSON parse error: " << e.what()
                      << " | Payload: " << payload << std::endl;
        }
    }
};

// SSDP listener thread (osluskuje NOTIFY)
void ssdpListenerThread() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket error");
        return;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SSDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind error");
        close(sock);
        return;
    }

    ip_mreq mreq{};
    inet_pton(AF_INET, SSDP_ADDR, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("Multicast join error");
        close(sock);
        return;
    }

    char buffer[1024];
    while (true) {
        int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            std::string msg(buffer);

            // trazi NOTIFY od mikrokontrolera
            if (msg.find("uuid:microcontroller-001") != std::string::npos &&
                msg.find("LOCATION:") != std::string::npos) {

                auto pos = msg.find("LOCATION:");
                std::string locLine = msg.substr(pos);
                auto end = locLine.find("\r\n");
                if (end != std::string::npos) locLine = locLine.substr(0, end);

                std::string url = locLine.substr(9); // izbaci "LOCATION:"
                url.erase(0, url.find_first_not_of(" \t")); // trim

                // Pretvori http:// u tcp://
               if (url.rfind("http://", 0) == 0) url.replace(0, 7, "tcp://");

                // Parsiraj host i port
                size_t slashPos = url.find('/', 6); // posle "tcp://"
                if (slashPos != std::string::npos)
                    url = url.substr(0, slashPos);

                // Ako nedostaje port, dodaj 1883
                if (url.find(':', 6) == std::string::npos)
                    url += ":1883";

                {
                    std::lock_guard<std::mutex> lock(serverMutex);
                    if (SERVER_ADDRESS != url) {
                        SERVER_ADDRESS = url;
                        std::cout << "[SSDP] MQTT broker found at " << SERVER_ADDRESS << std::endl;
                    }
                }
            }
        }
    }

    close(sock);
}

int main() {
    std::cout << "VENTILATOR" << std::endl;

    // Pokreni SSDP listener
    std::thread ssdpThread(ssdpListenerThread);

    // Cekaj dok ne dobijemo SERVER_ADDRESS iz SSDP
    while (true) {
        {
            std::lock_guard<std::mutex> lock(serverMutex);
            if (!SERVER_ADDRESS.empty()) break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);
    FanCallback cb(&client);
    client.set_callback(cb);

    try {
        client.connect()->wait();
        client.subscribe("actuators/fan", 1)->wait();
        std::cout << "[VENTILATOR] Subscribed to 'actuators/fan'" << std::endl;

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        client.disconnect()->wait();
    } catch (const mqtt::exception& exc) {
        std::cerr << "MQTT Error: " << exc.what() << std::endl;
        return 1;
    }

    ssdpThread.join();
    return 0;
}
