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
#include "main.h"
#include "election.h"
#include "lockManager.h"

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

bool node_equals(const std::pair<string, int>& a, const std::pair<string, int>& b){
    return a.first == b.first && a.second == b.second;
}
void printMsgOrigin(std::shared_ptr<networkMessage> msg){
    cout<<"message origin: "<<msg->data<<endl;
}

std::set<std::pair<std::string, int>> hosts;
std::unordered_map<std::pair<string, int>, std::shared_ptr<RemoteConnection>, pairhash> connections;

void leaderChanged(const std::pair<std::string, int>& remote){
    lockManager::leaderChanged(remote);
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
        con->registerCallback(&election::handleElection, MessageType_t::ELECTION);
        con->registerCallback(&election::handleAck, MessageType_t::ACK);
        con->registerCallback(&election::handleCoord, MessageType_t::COORDINATOR);
        con->registerTimeoutCallback(&election::handleTimeout);
        connections[host] = con;
    }
    {
        auto host = getHost();
        BOOST_LOG_SEV(lg, normal) << "Starting connection to local node: " << host.first <<":"<<host.second;
        cout<<"connecting to host: "<<host.first<<":"<<host.second<<endl;
        auto con = std::make_shared<RemoteConnection>(&ios, host.first, host.second);
        con->registerTimeoutCallback([](const std::pair<std::string, int>& remote){cerr<<"loopback connection timed out"<<endl;});
        connections[host] = con;
    }
    election::setLeaderChange(leaderChanged);


    std::thread r([&] { ios.run(); });


    usleep(1000000ul * HEARTBEAT_TIMEOUT);
    while(!(bool)election::election_leader) {
        if (!election::election_leader) { //join network; if no leader broadcast was received in the last 2*TIMEOUT interval start election
            election::startElection();
            usleep(1000000ul);
        }

        cout << "has leader: " << (bool) election::election_leader << endl;
    }
    if(argc == 2){
        election::startElection();
    }
    //lockManager::leaderChanged(*election::election_leader);
    usleep(250);

    char inp = ' ';
    std::cout<<"Select action: "<<endl<<"c: crash node" << endl << "q: quit node" <<endl<< "l: acquire lock and unlock immediately" <<endl;
    while(inp != 'c'){
        if(std::cin >> inp) {
            if (inp == 'q') {
                break;
            }
            switch (inp) {
                case 'l':
                    lockManager::getLock([]() { std::cout << ">>>>>>>>>>>>>got lock<<<<<<<<<<" << std::endl; });
                    lockManager::getLock(lockManager::unlock);
                    break;
                default:
                    break;
            }
            std::cout<<"Select action: "<<endl<<"c: crash node" << endl << "q: quit node" <<endl<< "l: acquire lock and unlock immediately" <<endl;
        }else{
            usleep(1000000ul*20);
        }
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
