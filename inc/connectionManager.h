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
#include <set>
#include <functional>
#include "networkMessage.h"
#include "logging.h"

#define HEARTBEAT_INTERVAL 2ul
#define HEARTBEAT_TIMEOUT (2.5*HEARTBEAT_INTERVAL)

typedef std::function<void(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote)> callback_t;
typedef std::function<void(const std::pair<std::string, int>& remote)> timeout_callback_t;

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
    friend class RemoteConnection;
    std::deque<std::shared_ptr<networkMessage>> queue;
    boost::asio::io_service* io_service;
    boost::asio::ip::udp::socket *socket;
    boost::array<char, MSG_SZ> recv_buffer;
    boost::asio::ip::udp::endpoint remote_endpoint;
    boost::asio::deadline_timer deadline_;
    boost::asio::deadline_timer queue_notification;
    std::string remoteName;
    std::pair<std::string, int> remoteHost;
    std::unordered_map<int, callback_t> callbacks;
    timeout_callback_t timeout_cb;
    bool alive = true;
    void check_deadline();
public:
    Client(boost::asio::io_service* ios, std::string remote_ip,
           int remote_port, boost::asio::ip::udp::socket* rx_sock):deadline_(*ios),io_service(ios),
           queue_notification(*ios), socket(rx_sock), remoteName(remote_ip), remoteHost(remote_ip, remote_port) {
        deadline_.expires_at(boost::posix_time::pos_infin);
        check_deadline();//init callbacks
        boost::asio::ip::udp::resolver resolver(*io_service);
        boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), remote_ip, std::to_string(remote_port));
        boost::asio::ip::udp::resolver::iterator iter = resolver.resolve(query);
        remote_endpoint=*iter;
        queue_notification.expires_at(boost::posix_time::pos_infin);
        queue_notification.async_wait(boost::bind(&Client::queueEvent, this));
    }
    ~Client(){
        boost::system::error_code ignored_ec;
        queue_notification.cancel();
        deadline_.cancel();
    }
    int queuedMessages(){
        return queue.size();
    }
    void expire_deadline();
    std::shared_ptr<networkMessage> getMessage(){
        auto msg = queue.front();
        queue.pop_front();
        return msg;
    }
    bool isAlive(){
        return alive;
    }
    void registerCallback(callback_t cb, MessageType_t msgType){
        callbacks[static_cast<int>(msgType)] = cb;
    }
    void unregisterCallback(MessageType_t msgType){
        callbacks.erase(static_cast<int>(msgType));
    }
    void registerTimeoutCallback(timeout_callback_t cb) {
        timeout_cb = cb;
    }
    void handle_receive(std::shared_ptr<networkMessage> msg);
    void wait();
    void Receiver();
    void queueEvent();
    boost::asio::ip::udp::endpoint& getEndpoint(){
        return remote_endpoint;
    }
};

class election;
class RemoteConnection{
    friend class election;
    boost::array<char, MSG_SZ> rx_buffer;
    boost::asio::ip::udp::endpoint rx_endpoint;
    Sender sender;
    std::shared_ptr<Client> client;
    static boost::asio::io_service* io_service;
    static bool initialised;
    static boost::asio::ip::udp::socket *rx_socket;
    static std::set<std::pair<boost::asio::ip::udp::endpoint, std::shared_ptr<Client>>> endpointMap;
    boost::asio::deadline_timer nextHeartbeat;
    bool alive = false;
    std::string remoteName;
public:
    bool isEphemeral = false;
    RemoteConnection(boost::asio::io_service* ios, std::string ip, int port):sender(ios, ip, port),
                     nextHeartbeat(*ios), remoteName(ip) {
        client = std::make_shared<Client>(ios, ip, port, rx_socket);
        client->Receiver();
        endpointMap.insert(std::pair<boost::asio::ip::udp::endpoint, std::shared_ptr<Client>>(client->getEndpoint(), client));
        enableHeartbeat();
        if(!initialised){
            initialised=true;
            receive();
        }
    }
    RemoteConnection(std::string ip, int port):RemoteConnection(io_service, ip, port){}
    ~RemoteConnection(){
        BOOST_LOG_SEV(Logger::getLogger(), debug)<< "Connections: Client connection to "<<client->remoteHost.first<<":"<<client->remoteHost.second<<"killed"<<std::endl;
        endpointMap.erase(std::pair<boost::asio::ip::udp::endpoint, std::shared_ptr<Client>>(client->getEndpoint(), client));
        nextHeartbeat.cancel();
    }
    void sendMessage(std::shared_ptr<networkMessage> msg){
        resetHeartbeat();
        sender.put(msg);
    }
    std::shared_ptr<networkMessage> getMessage(){
        return client->getMessage();
    }
    int queuedRxMessages(){
        return client->queuedMessages();
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
        return client->isAlive();
    }
    void registerCallback(callback_t cb, MessageType_t msgType) {
        client->registerCallback(cb, msgType);
    }
    void unregisterCallback(MessageType_t msgType) {
        client->unregisterCallback(msgType);
    }
    void registerTimeoutCallback(timeout_callback_t cb) {
        client->registerTimeoutCallback(cb);//([this, cb](const std::pair<std::string, int>& remote){cb(remote); this->handleTimeout();});
    }
    void enableHeartbeat();
    void doHeartbeat();
    void resetHeartbeat();
    void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);
    void receive();
    void handleTimeout();
};

#endif //CS4103_CONNECTIONMANAGER_H
