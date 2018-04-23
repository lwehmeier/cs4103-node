//
// Created by leon on 11/04/18.
//

#ifndef CS4103_MAIN_H
#define CS4103_MAIN_H
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

using namespace boost;
using namespace std;

struct pairhash {
public:
    template <typename T, typename U>
    std::size_t operator()(const std::pair<T, U> &x) const
    {
        return std::hash<T>()(x.first) ^ std::hash<U>()(x.second);
    }
};

extern std::set<std::pair<std::string, int>> hosts;
extern std::unordered_map<std::pair<string, int>, std::shared_ptr<RemoteConnection>, pairhash> connections;
extern std::string DATABASE_PATH;
extern std::string NETWORKGRAPH_PATH;
extern std::string LOG_SRV;
//utils
bool node_equals(const std::pair<string, int>& a, const std::pair<string, int>& b);
void printMsgOrigin(std::shared_ptr<networkMessage> msg);
#endif //CS4103_MAIN_H
