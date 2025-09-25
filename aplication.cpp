#include <iostream>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>
#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using json = nlohmann::json;

double temperature = 0.0;
int co = 0;
int fanSpeed = 0;
bool buzzerOn = false;
std::mutex data_mutex;

std::string SERVER_ADDRESS; // dobija se iz SSDP
const std::string CLIENT_ID("ApplicationMonitor");

// SSDP parametri
constexpr const char* SSDP_ADDR = "239.255.255.250";
constexpr int SSDP_PORT = 1900;

// Callback za MQTT poruke
class AppCallback : public virtual mqtt::callback {
public:
    void message_arrived(mqtt::const_message_ptr msg) override {
        std::string topic = msg->get_topic();
        std::string payload = msg->to_string();

        try {
            json j = json::parse(payload);
            std::lock_guard<std::mutex> lock(data_mutex);

            if (topic == "controler/temp" && j.contains("temp")) {
                temperature = j["temp"].get<double>();
            }
            else if (topic == "controler/co" && j.contains("co")) {
                co = j["co"].get<int>();
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

// SSDP listener thread
void ssdpListenerThread() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0){ perror("Socket"); return; }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SSDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0){ perror("Bind"); close(sock); return; }

    ip_mreq mreq{};
    inet_pton(AF_INET, SSDP_ADDR, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    if(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0){
        perror("Multicast join"); close(sock); return;
    }

    char buffer[1024];
    bool gotServer = false;

    while(!gotServer){
        int n = recv(sock, buffer, sizeof(buffer)-1, 0);
        if(n>0){
            buffer[n] = '\0';
            std::string msg(buffer);

            if(msg.find("uuid:microcontroller-001") != std::string::npos &&
               msg.find("LOCATION:") != std::string::npos){

                auto pos = msg.find("LOCATION:");
                std::string locLine = msg.substr(pos);
                auto end = locLine.find("\r\n");
                if(end != std::string::npos) locLine = locLine.substr(0,end);

                std::string url = locLine.substr(9);
                url.erase(0,url.find_first_not_of(" \t"));
                url.erase(url.find_last_not_of(" \t")+1);

                if(url.rfind("http://",0)==0) url.replace(0,7,"tcp://");

                size_t slashPos = url.find('/',6);
                if(slashPos != std::string::npos) url = url.substr(0,slashPos);

                if(url.find(':',6)==std::string::npos) url += ":1883";

                {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    SERVER_ADDRESS = url;
                    std::cout << "[SSDP] MQTT broker found at " << SERVER_ADDRESS << std::endl;
                    gotServer = true;
                }
            }
        }
    }

    close(sock);
}

int main() {
    // Pokreni SSDP listener
    std::thread(ssdpListenerThread).detach();

    // Cekaj dok ne dobijemo SERVER_ADDRESS
    while(true){
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            if(!SERVER_ADDRESS.empty()) break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);
    AppCallback cb;
    client.set_callback(cb);

    try {
        client.connect()->wait();

        client.subscribe("controler/temp", 1)->wait();
        client.subscribe("controler/co", 1)->wait();
        client.subscribe("actuators/fan", 1)->wait();
        client.subscribe("actuators/buzzer", 1)->wait();

        while (true) {
            std::cout << "\033[2J\033[1;1H"; // clear screen

            {
                std::lock_guard<std::mutex> lock(data_mutex);
                std::cout << "==============================" << std::endl;
                std::cout << " TEMPERATURE & CO MONITOR " << std::endl;
                std::cout << "==============================" << std::endl;
                std::cout << "Temperature: " << temperature << " °C" << std::endl;
                std::cout << "CO Level: " << co << " ppm" << std::endl;
                std::cout << "Fan Speed: " << fanSpeed << " %" << std::endl;
                std::cout << "Buzzer: " << (buzzerOn ? "ON" : "OFF") << std::endl;
                std::cout << "==============================" << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        client.disconnect()->wait();
    } catch (const mqtt::exception& exc) {
        std::cerr << "MQTT Error: " << exc.what() << std::endl;
        return 1;
    }
}
