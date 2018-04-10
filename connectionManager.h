//
// Created by lw96 on 06/04/18.
//

#ifndef CS4103_CONNECTIONMANAGER_H
#define CS4103_CONNECTIONMANAGER_H
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
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <iostream>
#include <deque>
#include "networkMessage.h"

#define HEARTBEAT_INTERVAL 2ul
#define HEARTBEAT_TIMEOUT (2.5*HEARTBEAT_INTERVAL)

class Sender{
    std::deque<std::shared_ptr<networkMessage>> queue;
    boost::asio::io_service* io_service;
    boost::asio::ip::udp::socket socket{*io_service};
    boost::asio::ip::udp::endpoint remote_endpoint;
    boost::asio::deadline_timer queue_notification;
    std::string remoteName;
public:
    Sender(boost::asio::io_service* ios, std::string target_ip, int target_port) : io_service(ios), queue_notification(*ios), remoteName(target_ip)
    {
        boost::asio::ip::udp::resolver resolver(*io_service);
        boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), target_ip, std::to_string(target_port));
        boost::asio::ip::udp::resolver::iterator iter = resolver.resolve(query);
        remote_endpoint = *iter;
        // The non_empty_output_queue_ deadline_timer is set to pos_infin whenever
        // the output queue is empty. This ensures that the output actor stays
        // asleep until a message is put into the queue.
        queue_notification.expires_at(boost::posix_time::pos_infin);
        queue_notification.async_wait(boost::bind(&Sender::queueEvent, this));
        socket.open(boost::asio::ip::udp::v4());
    }
    ~Sender(){
        if(socket.is_open()) {
            socket.cancel();
        }
        queue_notification.cancel();
    }
    void send();
    void queueEvent();
    void put(std::shared_ptr<networkMessage> msg);
};

class Client {
    std::deque<std::shared_ptr<networkMessage>> queue;
    boost::asio::io_service* io_service;
    boost::asio::ip::udp::socket *socket;
    boost::array<char, MSG_SZ> recv_buffer;
    boost::asio::ip::udp::endpoint remote_endpoint;
    boost::asio::deadline_timer deadline_;
    boost::asio::deadline_timer queue_notification;
    std::string remoteName;
    bool alive = true;
    void check_deadline();
public:
    Client(boost::asio::io_service* ios, std::string remote_ip,
           int remote_port, boost::asio::ip::udp::socket* rx_sock):deadline_(*ios),io_service(ios),
           queue_notification(*ios), socket(rx_sock), remoteName(remote_ip) {
        deadline_.expires_at(boost::posix_time::pos_infin);
        check_deadline();//init callbacks
        //socket.open(boost::asio::ip::udp::v4());
        boost::asio::ip::udp::resolver resolver(*io_service);
        boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), remote_ip, std::to_string(remote_port));
        boost::asio::ip::udp::resolver::iterator iter = resolver.resolve(query);
        remote_endpoint=*iter;
        //socket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), remote_port));
        // The non_empty_output_queue_ deadline_timer is set to pos_infin whenever
        // the output queue is empty. This ensures that the output actor stays
        // asleep until a message is put into the queue.
        queue_notification.expires_at(boost::posix_time::pos_infin);
        queue_notification.async_wait(boost::bind(&Client::queueEvent, this));
    }
    ~Client(){
        boost::system::error_code ignored_ec;
        //if(socket.is_open()) {
        //    socket.cancel(ignored_ec);
        //}
        queue_notification.cancel();
        deadline_.cancel();
    }
    int queuedMessages(){
        return queue.size();
    }
    std::shared_ptr<networkMessage> getMessage(){
        auto msg = queue.front();
        queue.pop_front();
        return msg;
    }
    bool isAlive(){
        return alive;
    }
    void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);
    void wait();
    void Receiver();
    void queueEvent();
};




class RemoteConnection{
    Sender sender;
    Client client;
    static boost::asio::io_service* io_service;
    static boost::asio::ip::udp::socket *rx_socket;
    boost::asio::deadline_timer nextHeartbeat;
    bool alive = false;
    std::string remoteName;
public:
    RemoteConnection(boost::asio::io_service* ios, std::string ip, int port):sender(ios, ip, port),
                     client(ios, ip, port, rx_socket), nextHeartbeat(*ios), remoteName(ip) {
        client.Receiver();
        enableHeartbeat();
    }
    void sendMessage(std::shared_ptr<networkMessage> msg){
        resetHeartbeat();
        sender.put(msg);
    }
    std::shared_ptr<networkMessage> getMessage(){
        return client.getMessage();
    }
    int queuedRxMessages(){
        return client.queuedMessages();
    }
    static void setup(boost::asio::io_service* ios, int rxPort){
        io_service = ios;
        if(!rx_socket) {
            rx_socket= new boost::asio::ip::udp::socket(*ios);
            rx_socket->open(boost::asio::ip::udp::v4());
            rx_socket->bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), rxPort));
        }
    }
    bool isAlive(){
        return client.isAlive();
    }
    void enableHeartbeat();
    void doHeartbeat();
    void resetHeartbeat();
};

#endif //CS4103_CONNECTIONMANAGER_H
