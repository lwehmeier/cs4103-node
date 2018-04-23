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
    BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: highest received metric: "<< ret <<std::endl;
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
    BOOST_LOG_SEV(Logger::getLogger(), info)<<"Election: starting election" <<std::endl;
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
        electionActive = true;
        election_q = nullptr;
        if(election_leader)
            connections[*election_leader]->client->expire_deadline();
        election_leader = nullptr;
        election_init = false;
        election_acks.clear();
    }
    if(!election_q && !election_init){
        BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: received election message. Setting parent to "<<parent.first<<":"<<parent.second<<endl;
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
            BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: received election message. Leaf node, sending ack to "<<parent.first<<":"<<parent.second<<endl;
            auto msg = std::make_shared<networkMessage>();
            msg->type = static_cast<int>(MessageType_t::ACK);
            sprintf(msg->data, "%s:%d:%d", getIdentity().data(), getHost().second, getUptime());
            connections[parent]->sendMessage(msg);
        }
    } else{
        BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: received election message. Already has parent, sending ack to "<<parent.first<<":"<<parent.second<<endl;
        auto msg = std::make_shared<networkMessage>();
        msg->type = static_cast<int>(MessageType_t::ACK);
        sprintf(msg->data, "%s:%d:%d", getIdentity().data(), getHost().second, getUptime());
        connections[parent]->sendMessage(msg);
    }
}
void election::handleAck(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote){
    if(!electionActive){
        BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: received unexpected election ack from "<<remote.first<<":"<<remote.second<<endl;
        return;
    }
    BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: received ack message. from "<<remote.first<<":"<<remote.second<<endl;
    election_acks[remote]=std::pair<bool, std::string>(true, std::string(rxMessage->data));
    for(auto [h, ack] : election_acks){
        if(!ack.first){
            return;
        }
    }
    if(election_q) {
        BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: received acks from all adjacent nodes. Sending ack to parent: " << election_q->first << ":"
                                                 << election_q->second << endl;
        election_acks[getHost()]=std::pair<bool, std::string>(true, std::string(getIdentity() +std::string(":")+ std::to_string(getHost().second) +std::string(":") + std::to_string(getUptime())));
        auto msg = std::make_shared<networkMessage>();
        msg->type = static_cast<int>(MessageType_t::ACK);
        sprintf(msg->data, getHighestMetric().data());
        connections[*election_q]->sendMessage(msg);
    } else{
        BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: received acks from all adjacent nodes. Election done" << election_init << endl;
        if(election_init){
            election_acks[getHost()]=std::pair<bool, std::string>(true, std::string(getIdentity() +std::string(":")+ std::to_string(getHost().second) +std::string(":") + std::to_string(getUptime())));
            auto msg = std::make_shared<networkMessage>();
            msg->type = static_cast<int>(MessageType_t::COORDINATOR);
            std::string newLeader = getHighestMetric();
            sprintf(msg->data, newLeader.data());
            string addr = newLeader.substr(0, strcspn(newLeader.data(), ":"));
            string port = newLeader.substr(strcspn(newLeader.data(), ":")+1);
            port = port.substr(0, strcspn(port.data(), ":"));
            int parsedPort = atoi(port.data());
            election_leader = new std::pair<string, int>(addr.data(), parsedPort);
            electionActive = false;
            if(node_equals(*election_leader, getHost())){
                isLeader = true;
                BOOST_LOG_SEV(Logger::getLogger(), info)<<"Election: I am the new leader"<<std::endl;
            } else {
                isLeader = false;
            }
            createLeaderConIfNeeded();
            leaderchange(*election_leader);
            BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: broadcasting new leader"<<election_leader->first<<":"<<election_leader->second<<std::endl;
            for(std::pair<string, int> host: hosts) {
                if(connections[host]->isAlive()) {
                    connections[host]->sendMessage(msg);
                }
            }
        }
    }
}
void election::createLeaderConIfNeeded(){
    //kill ephemeral connections
    for(auto [host, con] : connections){
        if(con->isEphemeral){
            BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Post Election: closing ephemeral connection to " << host.first << ":"
                    << host.second << endl;
            connections.erase(host);
        }
    }
    //setup new connections
    if(!isLeader) { //init connection to leader
        auto iter = connections.find(*election_leader);
        if (iter == connections.end()) {
            BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Post Election: Starting ephemeral connection to unconnected master node: "
                                      << election_leader->first << ":" << election_leader->second << std::endl;
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
        BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Post Election: Starting ephemeral client connections" << endl;
        for(auto host : getAllHosts()){
            auto iter = connections.find(host);
            if (iter == connections.end()) {
                BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Post Election: Starting ephemeral connection to unconnected client node: "
                                          << host.first << ":" << host.second << endl;
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

    auto ldr = new std::pair<string, int>(addr, parsedPort);
    if(election_leader && node_equals(*ldr, *election_leader)){
        BOOST_LOG_SEV(Logger::getLogger(), trace)<<"Election: received new leader: " << addr <<":"<<parsedPort<<std::endl;
        BOOST_LOG_SEV(Logger::getLogger(), trace)<<"Election: leader already known, stopping"<<std::endl;
        return;
    }
    BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: received new leader: " << addr <<":"<<parsedPort<<std::endl;
    election_leader = ldr;
    if(node_equals(*election_leader, getHost())){
        isLeader = true;
        BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: I am the new leader"<<std::endl;
    } else {
        isLeader = false;
    }
    createLeaderConIfNeeded();
    leaderchange(*election_leader);
    BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Election: broadcasting new leader"<<ldr->first<<":"<<ldr->second<<std::endl;
    for(std::pair<string, int> host: hosts) {
        if(!node_equals(host,remote) && connections[host]->isAlive()) {
            connections[host]->sendMessage(rxMessage);
        }
    }
}

void election::handleTimeout(const std::pair<std::string, int>& remote) {
    if(isLeader){
        int activeConnections=0;
        for(auto [host, con] : connections){
            if(con->isAlive()){
                BOOST_LOG_SEV(Logger::getLogger(), trace)<<"Connections: Connection is alive: " << host.first << ":" << host.second << endl;
                activeConnections++;
            }
        }
        if(activeConnections <= getAllHosts().size()/2){
            BOOST_LOG_SEV(Logger::getLogger(), error)<<"Master: Lost connections to majority of network. Shutting down to avoid network split" << endl;
            std::terminate();
        }
    }
    if(election_leader && !electionActive && node_equals(remote, *election_leader)) {
        startElection();
    }
}
void election::setLeaderChange(leaderchange_callback_t cb){
    leaderchange = cb;
}