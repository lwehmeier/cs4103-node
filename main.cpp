//
// daytime_client.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <array>
#include <future>
#include <iostream>
#include <thread>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/use_future.hpp>
#include "networkParser.h"
#include "connectionManager.h"

using namespace boost;
using namespace std;



int main(int argc, char* argv[])
{
    readGraph();
    boost::asio::io_service ios;
    tcp::resolver r(ios);

    client c(ios);
    c.start(r.resolve(tcp::resolver::query("pc2-104-l","12345")));
    client c2(ios);
    c2.start(r.resolve(tcp::resolver::query("pc2-104-l","12345")));


    tcp::endpoint listen_endpoint(tcp::v4(), 12345);
    server s(ios, listen_endpoint);

    ios.run();

    return 0;
}