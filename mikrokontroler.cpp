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
std::mutex data_mutex;

// Funkcije ventilatora i sirene
int calculateFanSpeed(double temp, int co) {
    int speed = 0;

    // Pocinje da radi ako je temperatura > 35
    if(temp > 35) 
        speed = static_cast<int>((temp - 35) * 10);

    // Pojačava se ako je co > 800 ppm
    if(co > 800) {
        int s = (co - 800) / 4;
        if(s > speed) speed = s;
    }

    // Maksimalna snaga ventilatora = 100%
    if(speed > 100) speed = 100;

    return speed;
}

bool shouldSoundBuzzer(double temp, int co) {
    return temp > 40 || co > 1000;
}

// SSDP NOTIFY thread
void ssdpNotify(const std::string& usn, const std::string& nt){
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &addr.sin_addr);

    std::string msg = 
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "NT: " + nt + "\r\n"
        "NTS: ssdp:alive\r\n"
        "USN: " + usn + "\r\n"
        "LOCATION: http://10.1.207.138/desc.xml\r\n\r\n";

    while(true){
        sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&addr, sizeof(addr));
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    close(sock);
}

// MQTT callback
class SensorCallback : public virtual mqtt::callback {
public:
    void message_arrived(mqtt::const_message_ptr msg) override {
        std::string payload = msg->to_string();
        try {
            json j = json::parse(payload);
            if(j.contains("sensor") && j.contains("value")){
                std::string type = j["sensor"].get<std::string>();
                std::lock_guard<std::mutex> lock(data_mutex);
                if(type=="temperature") temperature = j["value"].get<double>();
                else if(type=="co") co = j["value"].get<int>();
                std::cout<<"[MQTT-JSON] Received: "<<j.dump()<<std::endl;
            }
        } catch(const std::exception& e){
            std::cerr<<"JSON parse error: "<<e.what()<<" | Payload: "<<payload<<std::endl;
        }
    }
};

int main(){
    std::cout<<"MIKROKONTROLER"<<std::endl;
    const std::string SERVER_ADDRESS("tcp://10.1.145.71");
    const std::string CLIENT_ID("Microcontroller");

    mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);
    SensorCallback cb;
    client.set_callback(cb);

    std::thread(ssdpNotify, "uuid:microcontroller-001", "urn:schemas-upnp-org:device:Controller:1").detach();

    try {
        client.connect()->wait();
        client.subscribe("sensors/temperature",1)->wait();
        client.subscribe("sensors/co",1)->wait();

        while(true){
            std::cout<<"==========================================="<<std::endl;
            int fanSpeed;
            bool buzzerOn;
            {
                std::lock_guard<std::mutex> lock(data_mutex);
                fanSpeed = calculateFanSpeed(temperature, co);
                buzzerOn = shouldSoundBuzzer(temperature, co);
            }

            json fanMsg = { {"fanSpeed", fanSpeed} };
            client.publish("actuators/fan", fanMsg.dump(), 1, false);

            json coMsg = { {"co", co} };
            client.publish("controler/co", coMsg.dump(), 1, false);

            json tempMsg = { {"temp", temperature} };
            client.publish("controler/temp", tempMsg.dump(), 1, false);

            json buzzerMsg = { {"buzzer", buzzerOn} };
            client.publish("actuators/buzzer", buzzerMsg.dump(), 1, false);

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        client.disconnect()->wait();
    } catch(const mqtt::exception& exc){
        std::cerr<<"MQTT Error: "<<exc.what()<<std::endl;
        return 1;
    }
}
