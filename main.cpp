#include <array>
#include <future>
#include <iostream>
#include <thread>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/use_future.hpp>
#include "networkParser.h"
#include "connectionManager.h"
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "logging.h"
#include <stdio.h>
#include <chrono>
#include <fstream>

struct pairhash {
public:
    template <typename T, typename U>
    std::size_t operator()(const std::pair<T, U> &x) const
    {
        return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
    }
};

void handler(int sig) {
    void *array[10];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

using namespace boost;
using namespace std;

std::set<std::pair<std::string, int>> hosts;
std::unordered_map<std::pair<string, int>, std::shared_ptr<RemoteConnection>, pairhash> connections;

const std::pair<string, int> *election_q = nullptr;
const std::pair<string, int> *election_leader = nullptr;
bool election_init = false;
bool electionActive = false;
bool isLeader = false;
std::unordered_map<std::pair<string, int>, std::pair<bool, std::string>, pairhash> election_acks;

bool node_equals(const std::pair<string, int>& a, const std::pair<string, int>& b){
    return a.first == b.first && a.second == b.second;
}
void printMsgOrigin(std::shared_ptr<networkMessage> msg){
    cout<<"message origin: "<<msg->data<<endl;
}
std::string getHighestMetric(){
    std::string ret;
    int bestMetric=0;
    for(auto [h, ack] : election_acks){
        std::string data = ack.second;
        string metr = data.substr(strcspn(data.data(), ":")+1);
        metr = metr.substr(strcspn(metr.data(), ":")+1);
        int parsedValue = atoi(metr.data());
        if(parsedValue >= bestMetric){
            bestMetric = parsedValue;
            ret = data;
        }
    }
    cout<<"highest metric: " << ret << endl;
    return ret;
}
int getUptime(){
    std::chrono::milliseconds uptime(0u);
    long uptime_seconds;
    if (std::ifstream("/proc/uptime", std::ios::in) >> uptime_seconds)
    {
        uptime = std::chrono::milliseconds(
                static_cast<unsigned long long>(uptime_seconds)*1000ULL
        );
    }
    return uptime_seconds;
}
void startElection(){
    electionActive = true;
    election_q = nullptr;
    election_leader = nullptr;
    election_init = false;
    election_acks.clear();
    cout<<"starting election"<<endl;
    election_init = true;
    for(std::pair<string, int> host: hosts) {
        auto msg = std::make_shared<networkMessage>();
        msg->type= static_cast<int>(MessageType_t::ELECTION);
        sprintf(msg->data, getIdentity().data());
        if(connections[host]->isAlive()) {
            election_acks[host]=std::pair<bool, std::string>(false,"");
            connections[host]->sendMessage(msg);
        }
    }
}
void handleElection(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& parent){
    //printMsgOrigin(rxMessage);
    if(!electionActive){
        election_q = nullptr;
        election_leader = nullptr;
        election_init = false;
        election_acks.clear();
    }
    electionActive = true;
    if(!election_q && !election_init){
        cout<<"received election message. Setting parent to "<<parent.first<<":"<<parent.second<<endl;
        election_q = &parent;
        for(std::pair<string, int> host: hosts) {
            auto msg = std::make_shared<networkMessage>();
            msg->type = static_cast<int>(MessageType_t::ELECTION);
            sprintf(msg->data, getIdentity().data());
            if (connections[host]->isAlive() && !node_equals(*election_q,host)) {
                election_acks[host]=std::pair<bool, std::string>(false,"");
                connections[host]->sendMessage(msg);
            }
        }
        if(election_acks.size()==0){//"end"/leaf node, send ack
            cout<<"received election message. Leaf node, sending ack to "<<parent.first<<":"<<parent.second<<endl;
            auto msg = std::make_shared<networkMessage>();
            msg->type = static_cast<int>(MessageType_t::ACK);
            sprintf(msg->data, "%s:%d:%d", getIdentity().data(), getHost().second, getUptime());
            connections[parent]->sendMessage(msg);
        }
    } else{
        cout<<"received election message. Already has parent, sending ack to "<<parent.first<<":"<<parent.second<<endl;
        auto msg = std::make_shared<networkMessage>();
        msg->type = static_cast<int>(MessageType_t::ACK);
        sprintf(msg->data, "%s:%d:%d", getIdentity().data(), getHost().second, getUptime());
        connections[parent]->sendMessage(msg);
    }
}
void handleAck(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote){
    if(!electionActive){
        cout<<"received unexpected election ack from "<<remote.first<<":"<<remote.second<<endl;
        return;
    }
    //printMsgOrigin(rxMessage);
    cout<<"received ack message. from "<<remote.first<<":"<<remote.second<<endl;
    election_acks[remote]=std::pair<bool, std::string>(true, std::string(rxMessage->data));
    for(auto [h, ack] : election_acks){
        if(!ack.first){
            return;
        }
    }
    if(election_q) {
        cout << "received acks from all adjacent nodes. Sending ack to parent: " << election_q->first << ":"
             << election_q->second << endl;
        election_acks[getHost()]=std::pair<bool, std::string>(true, std::string(getIdentity() +std::string(":")+ std::to_string(getHost().second) +std::string(":") + std::to_string(getUptime())));
        auto msg = std::make_shared<networkMessage>();
        msg->type = static_cast<int>(MessageType_t::ACK);
        sprintf(msg->data, getHighestMetric().data());
        connections[*election_q]->sendMessage(msg);
    } else{
        cout << "received acks from all adjacent nodes. Election done" << election_init << endl;
        if(election_init){
            election_acks[getHost()]=std::pair<bool, std::string>(true, std::string(getIdentity() +std::string(":")+ std::to_string(getHost().second) +std::string(":") + std::to_string(getUptime())));
            auto msg = std::make_shared<networkMessage>();
            msg->type = static_cast<int>(MessageType_t::COORDINATOR);
            sprintf(msg->data, getHighestMetric().data());
            std::string newLeader = getHighestMetric();
            string addr = newLeader.substr(0, strcspn(newLeader.data(), ":"));
            string port = newLeader.substr(strcspn(newLeader.data(), ":")+1);
            port = port.substr(0, strcspn(port.data(), ":"));
            int parsedPort = atoi(port.data());
            election_leader = new std::pair<string, int>(addr.data(), parsedPort);
            electionActive = false;
            std::cout<<"broadcasting new leader"<<election_leader->first<<":"<<election_leader->second<<std::endl;
            if(node_equals(*election_leader, getHost())){
                isLeader = true;
                std::cout<<"I am the new leader"<<std::endl;
            } else {
                isLeader = false;
            }
            for(std::pair<string, int> host: hosts) {
                if(connections[host]->isAlive()) {
                    connections[host]->sendMessage(msg);
                }
            }
        }
    }
}
void handleCoord(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote){

    electionActive = false;

    std::string leader = std::string(rxMessage->data);
    string addr = leader.substr(0, strcspn(leader.data(), ":"));
    string port = leader.substr(strcspn(leader.data(), ":")+1);
    port = port.substr(0, strcspn(port.data(), ":"));
    int parsedPort = atoi(port.data());

    std::cout << "received new leader: " << addr <<":"<<parsedPort<<std::endl;
    auto ldr = new std::pair<string, int>(addr, parsedPort);
    if(election_leader && node_equals(*ldr, *election_leader)){
        std::cout<<"leader already known, stopping"<<std::endl;
        return;
    }
    election_leader = ldr;
    std::cout<<getHost().first<<getHost().second<<" == " << election_leader->first << election_leader -> second << node_equals(*election_leader, getHost()) << endl;
    if(node_equals(*election_leader, getHost())){
        isLeader = true;
        std::cout<<"I am the new leader"<<std::endl;
    } else {
        isLeader = false;
    }
    std::cout<<"broadcasting new leader"<<ldr->first<<":"<<ldr->second<<std::endl;
    for(std::pair<string, int> host: hosts) {
        if(!node_equals(host,remote) && connections[host]->isAlive()) {
            connections[host]->sendMessage(rxMessage);
        }
    }
}

void handleTimeout(const std::pair<std::string, int>& remote) {
    if(election_leader && !electionActive && node_equals(remote, *election_leader)) {
        startElection();
    }
}
int main(int argc, char* argv[])
{
    init_builtin_syslog();
    src::severity_logger< severity_levels > lg(keywords::severity = normal);
    BOOST_LOG_SEV(lg, normal) << "Application started";
    signal(SIGSEGV, handler);   // install our handler
    //signal(SIGABRT, handler);   // install our handler
    readGraph();
    hosts = getNeighbourHosts();
    cout<<"adjacent hosts:"<<endl;
    for(std::pair<string, int> host : hosts){
        cout<<"\t"<<host.first << ":" << host.second<<endl;
    }
    boost::asio::io_service ios;
    RemoteConnection::setup(&ios, getHost().second);

    for(std::pair<string, int> host : hosts) {
        BOOST_LOG_SEV(lg, normal) << "Starting connection to adjacent node: " << host.first <<":"<<host.second;
        cout<<"connecting to host: "<<host.first<<":"<<host.second<<endl;
        auto con = std::make_shared<RemoteConnection>(&ios, host.first, host.second);
        con->registerCallback(handleElection, MessageType_t::ELECTION);
        con->registerCallback(handleAck, MessageType_t::ACK);
        con->registerCallback(handleCoord, MessageType_t::COORDINATOR);
        con->registerTimeoutCallback(handleTimeout);
        connections[host] = con;
    }

    std::thread r([&] { ios.run(); });


    usleep(1000000ul * HEARTBEAT_TIMEOUT);
    if(!election_leader){ //join network; if no leader broadcast was received in the last 2*TIMEOUT interval start election
        startElection();
        usleep(1000000ul);
    }
    cout<<"has leader: "<<(bool)election_leader<<endl;
    if(argc == 2){
        startElection();
    }
/*
    while(1){
        if(!isLeader && !electionActive && (election_leader && !connections[*election_leader]->isAlive() || !election_leader)){ //let's catch some pointers
            cout<<"new election"<<endl;
            startElection();
        }
        for(const std::pair<string, int>& host: hosts) {
            if(connections[host]->isAlive() && connections[host]->queuedRxMessages()) {
                auto msg = connections[host]->getMessage();
                switch (msg->type){
                    case static_cast<int>(MessageType_t::HEARTBEAT) :
                        cerr<<"WARN: Heartbeat message in rx queue"<<host.first<<":"<<host.second<<endl;
                        break;
                    case static_cast<int>(MessageType_t::TEXT) :
                        cout<<"received text message from node"<<host.first<<":"<<host.second<<endl;
                        break;
                   case static_cast<int>(MessageType_t::ELECTION) :
                        //cout<<"received election message from node"<<host.first<<":"<<host.second<<endl;
                        handleElection(msg, host);
                        break;
                    case static_cast<int>(MessageType_t::ACK) :
                        //cout<<"received ack message from node"<<host.first<<":"<<host.second<<endl;
                        handleAck(msg, host);
                        break;
                    case static_cast<int>(MessageType_t::COORDINATOR) :
                        //cout<<"received coordinator message from node"<<host.first<<":"<<host.second<<endl;
                        handleCoord(msg, host);
                        break;
                    default:
                        cerr<<"WARN: Received unknown message of type: "<<msg->type<<" from "<<host.first<<":"<<host.second<<endl;
                        break;
                }
            }
        }
    }*/

    r.join();
    return 0;
}
