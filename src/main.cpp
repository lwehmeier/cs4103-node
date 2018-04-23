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
#include "visitorAccessMgr.h"
#include "clientHandler.h"
std::string DATABASE_PATH;
std::string NETWORKGRAPH_PATH;
std::string LOG_SRV;

void handler(int sig) {
    void *array[10];
    size_t size;
    size = backtrace(array, 10);
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

bool node_equals(const std::pair<string, int>& a, const std::pair<string, int>& b){
    return a.first == b.first && a.second == b.second;
}

std::set<std::pair<std::string, int>> hosts;
std::unordered_map<std::pair<string, int>, std::shared_ptr<RemoteConnection>, pairhash> connections;

void leaderChanged(const std::pair<std::string, int>& remote){
    BOOST_LOG_SEV(Logger::getLogger(), info)<<"Main: leader changed"<<std::endl;
    lockManager::leaderChanged(remote);
    if(election::isLeader) {
        VisitorAccessManager::init_leader(5);
    }
}
void parse_cli(int argc, char* argv[]){
    if(argc!=4){
        std::cout << "Usage: " << argv[0] << "<networkGraphFile> <visitorDatabasePath> <SyslogServer>" << std::endl;
        std::terminate();
    }
    DATABASE_PATH=std::string(argv[2]);
    NETWORKGRAPH_PATH=std::string(argv[1]);
    LOG_SRV=std::string(argv[3]);
}
int main(int argc, char* argv[])
{
    signal(SIGSEGV, handler);   // install our handler
    //signal(SIGABRT, handler);   // install our handler

    parse_cli(argc, argv);

    Logger::setup(LOG_SRV);

    VisitorAccessManager::setup(DATABASE_PATH);

    readGraph(NETWORKGRAPH_PATH);
    hosts = getNeighbourHosts();
    BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Main: adjacent hosts: "<<std::endl;
    for(std::pair<string, int> host : hosts){
        BOOST_LOG_SEV(Logger::getLogger(), debug)<<host.first << ":" << host.second<<std::endl;
    }
    boost::asio::io_service ios;
    lockManager::setup(ios);
    RemoteConnection::setup(&ios, getHost().second);

    for(std::pair<string, int> host : hosts) {
        BOOST_LOG_SEV(Logger::getLogger(), info)<<"Main: Starting connection to adjacent node: " << host.first <<":"<<host.second<<std::endl;
        auto con = std::make_shared<RemoteConnection>(&ios, host.first, host.second);
        con->registerCallback(&election::handleElection, MessageType_t::ELECTION);
        con->registerCallback(&election::handleAck, MessageType_t::ACK);
        con->registerCallback(&election::handleCoord, MessageType_t::COORDINATOR);
        con->registerTimeoutCallback(&election::handleTimeout);
        connections[host] = con;
    }
    {
        auto host = getHost();
        BOOST_LOG_SEV(Logger::getLogger(), info)<< "Main: Starting connection to local node: " << host.first <<":"<<host.second<<std::endl;
        auto con = std::make_shared<RemoteConnection>(&ios, host.first, host.second);
        con->registerTimeoutCallback([](const std::pair<std::string, int>& remote){cerr<<"loopback connection timed out"<<endl;});
        connections[host] = con;
    }

    election::setLeaderChange(leaderChanged);

    std::thread r([&] { ios.run(); });


    usleep(1000000ul * HEARTBEAT_TIMEOUT);
    while(!(bool)election::election_leader) {
        if (!election::election_leader) { //join network; if no leader broadcast was received in the last TIMEOUT interval start election
            election::startElection();
            usleep(1000000ul);
        }
        BOOST_LOG_SEV(Logger::getLogger(), debug)<< "Main: has leader: " << (bool) election::election_leader <<std::endl;
    }
    usleep(250);

    handleClient();
    std::terminate();
    r.join();
    return 0;
}
