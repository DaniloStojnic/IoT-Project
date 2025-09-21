#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>
#include "tunnel_simulator.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

using json = nlohmann::json;
const std::string SERVER_ADDRESS("tcp://localhost:1883");
const std::string CLIENT_ID("SensorSimulator");

bool microcontrollerFound = false;

void ssdpSearchThread(){
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &addr.sin_addr);

    std::string msg = "M-SEARCH * HTTP/1.1\r\n"
                      "HOST: 239.255.255.250:1900\r\n"
                      "MAN: \"ssdp:discover\"\r\n"
                      "MX: 3\r\n"
                      "ST: urn:schemas-upnp-org:device:Controller:1\r\n\r\n";

    sockaddr_in recvAddr{};
    socklen_t addrLen = sizeof(recvAddr);
    char buffer[1024];

    auto lastSeen = std::chrono::steady_clock::now();

    while(true){
        sendto(sock, msg.c_str(), msg.size(), 0, (sockaddr*)&addr, sizeof(addr));

        int n = recvfrom(sock, buffer, sizeof(buffer)-1, MSG_DONTWAIT, (struct sockaddr*)&recvAddr, &addrLen);
        if(n>0){
            buffer[n] = '\0';
            std::string resp(buffer);
            if(resp.find("uuid:microcontroller-001") != std::string::npos){
                lastSeen = std::chrono::steady_clock::now();
            }
        }

        if(std::chrono::steady_clock::now() - lastSeen > std::chrono::seconds(60)){
            std::cerr << "[SENSOR] Microcontroller not found! Shutting down..." << std::endl;
            exit(0); // isključenje uređaja
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}


int main(){
    std::cout<<"SENZOR"<<std::endl;
    TunnelSimulator tunnel;
    std::thread simThread(&TunnelSimulator::run, &tunnel);
    std::thread(ssdpSearchThread).detach();

    mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);

    try{
        client.connect()->wait();
        std::cout << "[SENSOR] Connected to MQTT broker." << std::endl;

        while(true){
            double temp = tunnel.getTemperature();
            int co = tunnel.getco();

            json tempJson = { {"sensor","temperature"},{"value",temp},{"unit","C"} };
            json coJson = { {"sensor","co"},{"value",co},{"unit","ppm"} };

            client.publish("sensors/temperature", tempJson.dump(),1,false);
            client.publish("sensors/co", coJson.dump(),1,false);
            std::cout<<"==========================================="<<std::endl;
            std::cout<<"[PUBLISH] "<<tempJson.dump()<<" | "<<coJson.dump()
                     <<" | Cars: "<<tunnel.getCars()<<std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        client.disconnect()->wait();
    } catch(const mqtt::exception& exc){
        std::cerr<<"MQTT Error: "<<exc.what()<<std::endl;
        return 1;
    }

    simThread.join();
}
