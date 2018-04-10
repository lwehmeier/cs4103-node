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


std::pair<string, int> *election_q = nullptr;
bool election_init = false;
std::unordered_map<std::pair<string, int>, bool, pairhash> election_acks;
bool node_equals(std::pair<string, int>& a, std::pair<string, int>& b){
    return a.first == b.first && a.second == b.second;
}
void printMsgOrigin(std::shared_ptr<networkMessage> msg){
    cout<<"message origin: "<<msg->data<<endl;
}
void startElection(){
    cout<<"starting election"<<endl;
    election_init = true;
    for(std::pair<string, int> host: hosts) {
        auto msg = std::make_shared<networkMessage>();
        msg->type= static_cast<int>(MessageType_t::ELECTION);
        sprintf(msg->data, getIdentity().data());
        if(connections[host]->isAlive()) {
            connections[host]->sendMessage(msg);
        }
    }
}
void handleElection(std::shared_ptr<networkMessage> rxMessage, std::pair<std::string, int>& parent){
    printMsgOrigin(rxMessage);
    if(!election_q && !election_init){
        cout<<"received election message. Setting parent to "<<parent.first<<":"<<parent.second<<endl;
        election_q = &parent;
        for(std::pair<string, int> host: hosts) {
            auto msg = std::make_shared<networkMessage>();
            msg->type = static_cast<int>(MessageType_t::ELECTION);
            sprintf(msg->data, getIdentity().data());
            if (connections[host]->isAlive() && !node_equals(*election_q, host)) {
                election_acks[host]=false;
                connections[host]->sendMessage(msg);
            }
        }
    } else{
        cout<<"received election message. Already has parent, sending ack to "<<parent.first<<":"<<parent.second<<endl;
        auto msg = std::make_shared<networkMessage>();
        msg->type = static_cast<int>(MessageType_t::ACK);
        sprintf(msg->data, getIdentity().data());
        connections[parent]->sendMessage(msg);
    }
}
void handleAck(std::shared_ptr<networkMessage> rxMessage, std::pair<std::string, int>& host){
    printMsgOrigin(rxMessage);
    cout<<"received ack message. from "<<host.first<<":"<<host.second<<endl;
    election_acks[host]=true;
    for(auto [h, ack] : election_acks){
        if(!ack){
            return;
        }
    }
    if(election_q) {
        cout << "received acks from all adjacent nodes. Sending ack to parent: " << election_q->first << ":"
             << election_q->second << endl;
        auto msg = std::make_shared<networkMessage>();
        msg->type = static_cast<int>(MessageType_t::ACK);
        sprintf(msg->data, getIdentity().data());
        connections[*election_q]->sendMessage(msg);
    } else{
        cout << "received acks from all adjacent nodes. Election done" << election_init << endl;
    }
}
void handleCoord(std::shared_ptr<networkMessage> rxMessage, std::pair<std::string, int>& host){

}
int main(int argc, char* argv[])
{
    init_builtin_syslog();
    src::severity_logger< severity_levels > lg(keywords::severity = normal);
    BOOST_LOG_SEV(lg, normal) << "Application started";
    //signal(SIGSEGV, handler);   // install our handler
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
        connections[host] = std::make_shared<RemoteConnection>(&ios, host.first, host.second);
    }

    std::thread r([&] { ios.run(); });

    /*for (int i = 0; i < 40; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        for(std::pair<string, int> host: hosts) {
            if(connections[host]->isAlive()) {
                auto msg = std::make_shared<networkMessage>();
                msg->type= static_cast<int>(MessageType_t::TEXT);
                connections[host]->sendMessage(msg);
            }
        }
    }
    */

    if(argc == 2){
        startElection();
    }

    while(1){
        for(std::pair<string, int> host: hosts) {
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
                        cout<<"received election message from node"<<host.first<<":"<<host.second<<endl;
                        handleElection(msg, host);
                        break;
                    case static_cast<int>(MessageType_t::ACK) :
                        cout<<"received ack message from node"<<host.first<<":"<<host.second<<endl;
                        handleAck(msg, host);
                        break;
                    case static_cast<int>(MessageType_t::COORDINATOR) :
                        cout<<"received coordinator message from node"<<host.first<<":"<<host.second<<endl;
                        handleCoord(msg, host);
                        break;
                    default:
                        cerr<<"WARN: Received unknown message of type: "<<msg->type<<" from "<<host.first<<":"<<host.second<<endl;
                        break;
                }
            }
        }
    }

    r.join();
    return 0;
}