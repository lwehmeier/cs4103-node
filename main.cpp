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

int main(int argc, char* argv[])
{
    init_builtin_syslog();
    src::severity_logger< severity_levels > lg(keywords::severity = normal);
    BOOST_LOG_SEV(lg, normal) << "Application started";
    //BOOST_LOG_SEV(lg, warning) << "A syslog record with warning level";
    //BOOST_LOG_SEV(lg, error) << "A syslog record with error level";
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

    for (int i = 0; i < 40; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        for(std::pair<string, int> host: hosts) {
            if(connections[host]->isAlive()) {
                auto msg = std::make_shared<networkMessage>();
                msg->type=0x42;
                connections[host]->sendMessage(msg);
            }
        }
        //std::cout<<"received messages in queue: "<<connections["localhost"]->queuedRxMessages()<<endl;
    }

    //connections.erase("localhost");

    r.join();

    return 0;
}