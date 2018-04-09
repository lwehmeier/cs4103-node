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

std::set<string> hosts;
std::unordered_map<string, std::shared_ptr<RemoteConnection>> connections;

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
    for(string s : hosts){
        cout<<"\t"<<s<<endl;
    }
    boost::asio::io_service ios;
    RemoteConnection::setup(&ios, 12345);

    for(string s : hosts) {
        BOOST_LOG_SEV(lg, normal) << "Starting connection to adjacent node: " << s;
        cout<<"connecting to host: "<<s<<endl;
        connections[s] = std::make_shared<RemoteConnection>(&ios, s, 12345);
    }

    std::thread r([&] { ios.run(); });

    for (int i = 0; i < 40; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        for(string s : hosts) {
            if(connections[s]->isAlive()) {
                auto msg = std::make_shared<networkMessage>();
                msg->type=0x42;
                connections[s]->sendMessage(msg);
            }
        }
        //std::cout<<"received messages in queue: "<<connections["localhost"]->queuedRxMessages()<<endl;
    }

    //connections.erase("localhost");

    r.join();

    return 0;
}