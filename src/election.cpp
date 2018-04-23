//
// Created by leon on 11/04/18.
//

#include "main.h"
#include "networkMessage.h"
#include "election.h"
#include <iostream>
#include <memory>
#include <tuple>
using namespace std;

const std::pair<string, int> *election::election_q = nullptr;
const std::pair<string, int> *election::election_leader = nullptr;
bool election::election_init = false;
bool election::electionActive = false;
bool election::isLeader = false;
leaderchange_callback_t election::leaderchange;
std::unordered_map<std::pair<string, int>, std::pair<bool, std::string>, pairhash> election::election_acks;

bool election::isCurrentLeader() {
        return isLeader;
    }
std::string election::getHighestMetric(){
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
int election::getUptime(){
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
void election::startElection(){
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
void election::handleElection(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& parent){
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
void election::handleAck(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote){
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
            if(node_equals(*election_leader, getHost())){
                isLeader = true;
                std::cout<<"I am the new leader"<<std::endl;
            } else {
                isLeader = false;
            }
            createLeaderConIfNeeded();
            leaderchange(*election_leader);
            std::cout<<"broadcasting new leader"<<election_leader->first<<":"<<election_leader->second<<std::endl;
            for(std::pair<string, int> host: hosts) {
                if(connections[host]->isAlive()) {
                    connections[host]->sendMessage(msg);
                }
            }
        }
    }
}
void election::createLeaderConIfNeeded(){
    //return;
    //clean up
    //kill ephemeral connections
    for(auto [host, con] : connections){
        if(con->isEphemeral){
            cout << "closing ephemeral connection to " << host.first << ":"
                 << host.second << endl;
            connections.erase(host);
            //hosts.erase(host);
        }
    }
    //setup new connections
    if(!isLeader) { //init connection to leader
        auto iter = connections.find(*election_leader);
        if (iter == connections.end()) {
            //hosts.insert(*election_leader);
            src::severity_logger<severity_levels> lg(keywords::severity = normal);
            BOOST_LOG_SEV(lg, normal) << "Starting ephemeral connection to unconnected master node: "
                                      << election_leader->first << ":" << election_leader->second << std::endl;
            cout << "Starting ephemeral connection to unconnected master node: " << election_leader->first << ":"
                 << election_leader->second << endl;
            auto con = std::make_shared<RemoteConnection>(election_leader->first, election_leader->second);
            con->registerCallback(&election::handleElection, MessageType_t::ELECTION);
            con->registerCallback(&election::handleAck, MessageType_t::ACK);
            con->registerCallback(&election::handleCoord, MessageType_t::COORDINATOR);
            con->registerTimeoutCallback(&election::handleTimeout);
            //con->isEphemeral = true;
            connections[*election_leader] = con;
        }
    }
    else{//create connections to clients
        cout << "Starting ephemeral client connections" << endl;
        for(auto host : getAllHosts()){
            cout << "checking node " << host.first << ":" << host.second << std::endl;
            auto iter = connections.find(host);
            if (iter == connections.end()) {
                //hosts.insert(host);
                src::severity_logger<severity_levels> lg(keywords::severity = normal);
                BOOST_LOG_SEV(lg, normal) << "Starting ephemeral connection to unconnected client node: "
                                          << host.first << ":" << host.second;
                cout << "Starting ephemeral connection to unconnected client node: " << host.first << ":"
                     << host.second << endl;
                auto con = std::make_shared<RemoteConnection>(host.first, host.second);
                con->registerCallback(&election::handleElection, MessageType_t::ELECTION);
                con->registerCallback(&election::handleAck, MessageType_t::ACK);
                con->registerCallback(&election::handleCoord, MessageType_t::COORDINATOR);
                con->registerTimeoutCallback(&election::handleTimeout);
                //con->isEphemeral = true;
                connections[host] = con;
            }
        }
    }
}
void election::handleCoord(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote){

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
    createLeaderConIfNeeded();
    leaderchange(*election_leader);
    std::cout<<"broadcasting new leader"<<ldr->first<<":"<<ldr->second<<std::endl;
    for(std::pair<string, int> host: hosts) {
        if(!node_equals(host,remote) && connections[host]->isAlive()) {
            connections[host]->sendMessage(rxMessage);
        }
    }
}

void election::handleTimeout(const std::pair<std::string, int>& remote) {
    if(election_leader && !electionActive && node_equals(remote, *election_leader)) {
        startElection();
    }
}
void election::setLeaderChange(leaderchange_callback_t cb){
    leaderchange = cb;
}