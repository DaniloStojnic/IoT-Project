#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <mqtt/async_client.h>
#include "tunnel_simulator.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <mutex>

using json = nlohmann::json;
const std::string CLIENT_ID("SensorSimulator");

std::string SERVER_ADDRESS; // dobija se iz SSDP
std::mutex serverMutex;

// SSDP listener thread
void ssdpListenerThread() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0){ perror("Socket"); return; }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1900);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0){ perror("Bind"); close(sock); return; }

    ip_mreq mreq{};
    inet_pton(AF_INET, "239.255.255.250", &mreq.imr_multiaddr);
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
               msg.find("LOCATION:") != std::string::npos) {

                auto pos = msg.find("LOCATION:");
                std::string locLine = msg.substr(pos);
                auto end = locLine.find("\r\n");
                if(end != std::string::npos) locLine = locLine.substr(0,end);

                std::string url = locLine.substr(9);
                url.erase(0,url.find_first_not_of(" \t"));
                url.erase(url.find_last_not_of(" \t")+1);

                // Pretvori http:// u tcp://
                if(url.rfind("http://",0)==0) url.replace(0,7,"tcp://");

                // Uzmi samo host:port
                size_t slashPos = url.find('/',6); // posle "tcp://"
                if(slashPos != std::string::npos) url = url.substr(0,slashPos);

                // Dodaj port ako nedostaje
                if(url.find(':',6)==std::string::npos) url += ":1883";

                {
                    std::lock_guard<std::mutex> lock(serverMutex);
                    SERVER_ADDRESS = url;
                    std::cout << "[SSDP] MQTT broker found at " << SERVER_ADDRESS << std::endl;
                    gotServer = true;
                }
            }
        }
    }

    close(sock);
}

int main(){
    std::cout<<"SENZOR"<<std::endl;

    TunnelSimulator tunnel;
    std::thread simThread(&TunnelSimulator::run, &tunnel);

    // Pokreni SSDP listener
    std::thread(ssdpListenerThread).detach();

    // Cekaj dok ne dobijemo SERVER_ADDRESS
    while(true){
        {
            std::lock_guard<std::mutex> lock(serverMutex);
            if(!SERVER_ADDRESS.empty()) break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

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
