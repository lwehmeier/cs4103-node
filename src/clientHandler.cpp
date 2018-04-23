//
// Created by lw96 on 22/04/18.
//

#include "clientHandler.h"
#include <boost/asio.hpp>
#include <boost/asio/read.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <thread>
#include <iostream>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <cstdlib>
#include <boost/bind.hpp>
#include <iostream>
#include <deque>
#include <set>
#include <functional>
#include "visitorAccessMgr.h"
#include "lockManager.h"

using boost::asio::ip::udp;
using boost::asio::ip::address;
using boost::asio::deadline_timer;
void handleClient(){
    boost::asio::io_service io_service;
    udp::socket socket(io_service, udp::endpoint(udp::v4(), CLIENTPORT));
    int ticket;
    for (;;)
    {
        boost::array<char, 256> recv_buf;
        udp::endpoint remote_endpoint;
        boost::system::error_code error;
        socket.receive_from(boost::asio::buffer(recv_buf),
                            remote_endpoint, 0, error);
        std::string message = "Select action:\r\nc: crash node\r\nq: quit node\r\ne <ticketID>: new entry\r\nl <ticketID>: leaving visitor\r\nv: print current visitors\r\n";
        boost::system::error_code ignored_error;
        socket.send_to(boost::asio::buffer(message),
                       remote_endpoint, 0, ignored_error);
        socket.receive_from(boost::asio::buffer(recv_buf),
                            remote_endpoint, 0, error);
        bool success, opFinished = 0;
        switch(recv_buf.data()[0]) {
            case 'c':
            case 'q':
                return;
            case 'e':
                ticket = atoi(recv_buf.data()+2);
                lockManager::getLock(std::bind([&success, &opFinished](int ticketID){
                    success = VisitorAccessManager::add_visitor(ticketID);
                    lockManager::unlock();
                    opFinished = 1;
                }, ticket));
                while(!opFinished);
                if (success) {
                    socket.send_to(boost::asio::buffer(
                            "Access Granted.\r\n"),
                                   remote_endpoint, 0, ignored_error);
                } else {
                    socket.send_to(boost::asio::buffer(
                            "Error. Could not grant access. Please retry later.\r\n"),
                                   remote_endpoint, 0, ignored_error);
                }
                break;
            case 'l':
                ticket = atoi(recv_buf.data()+2);
                lockManager::getLock(std::bind([&success, &opFinished](int ticketID){
                    success = VisitorAccessManager::remove_visitor(ticketID);
                    lockManager::unlock();
                    opFinished = 1;
                    }, ticket));
                while(!opFinished);
                if (success) {
                    socket.send_to(boost::asio::buffer("Leave Granted.\r\n"),
                                   remote_endpoint, 0, ignored_error);
                } else {
                    socket.send_to(boost::asio::buffer(
                            "Your ticket never entered the park and thus cannot leave. Security is on its way..\r\n"),
                                   remote_endpoint, 0, ignored_error);
                }
                break;
            case 'v':
                std::string vs = VisitorAccessManager::getCurrentVisitors();
                if (success) {
                    socket.send_to(boost::asio::buffer(
                            vs),
                                   remote_endpoint, 0, ignored_error);
                } else {
                    socket.send_to(boost::asio::buffer(
                            vs),
                                   remote_endpoint, 0, ignored_error);
                }
                break;
        }
    }
}