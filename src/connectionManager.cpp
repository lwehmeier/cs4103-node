#include <election.h>
#include "connectionManager.h"
#include "logging.h"
#include "networkParser.h"
#include "main.h"
using boost::asio::ip::udp;
using boost::asio::ip::address;
using boost::asio::deadline_timer;


void Sender::put(std::shared_ptr<networkMessage> msg) {
    queue.push_back(msg);
    queue_notification.expires_at(boost::posix_time::neg_infin);
}
void Sender::queueEvent() {
    if(queue.empty()){
        queue_notification.expires_at(boost::posix_time::pos_infin);
        queue_notification.async_wait(boost::bind(&Sender::queueEvent, this));
    } else{
        send();
    }
}
void Sender::send() {
    boost::system::error_code err;
    void* msg = new unsigned char[MSG_SZ];
    std::memcpy(msg, &(queue.front().get()->type),MSG_SZ);
    auto buffer = boost::asio::buffer(msg,MSG_SZ);
    auto sent = socket.send_to(buffer, remote_endpoint, 0, err);
    queue.pop_front();
    queueEvent();
}


void Client::queueEvent() {
    queue_notification.async_wait(boost::bind(&Client::queueEvent, this));
}
void Client::handle_receive(std::shared_ptr<networkMessage> msg) {
    if(callbacks.find(msg->type) != callbacks.end()){
        callbacks[msg->type](msg, remoteHost);
    }
    else {
        if (msg->type == 0) {
            BOOST_LOG_SEV(Logger::getLogger(), trace)<<"Connections: Received heartbeat from "<<remoteName <<std::endl;
        } else {
            BOOST_LOG_SEV(Logger::getLogger(), trace)<<"Connections: Received non-heartbeat message from "<<remoteName <<std::endl;
            queue.push_back(msg);
        }
    }
    alive = true;
    wait();
}

    void Client::wait() {
        deadline_.expires_from_now(boost::posix_time::seconds(HEARTBEAT_TIMEOUT));
    }

    void Client::Receiver(){
        wait();
    }
void Client::expire_deadline(){
    BOOST_LOG_SEV(Logger::getLogger(), info)<<"Connections: deadline expired for node "<<remoteName<<std::endl;
    alive = false;
    timeout_cb(remoteHost);
    deadline_.expires_at(boost::posix_time::pos_infin);
}
void Client::check_deadline()
{
    if (deadline_.expires_at() <= deadline_timer::traits_type::now() && socket->is_open())
    {
        expire_deadline();
    }
    deadline_.async_wait(boost::bind(&Client::check_deadline, this));
}


void RemoteConnection::enableHeartbeat() {
    resetHeartbeat();
    nextHeartbeat.async_wait(boost::bind(&RemoteConnection::doHeartbeat, this));
}
void RemoteConnection::doHeartbeat() {
    if (nextHeartbeat.expires_at() <= deadline_timer::traits_type::now()){
        resetHeartbeat();
        if(!election::isLeader) {
            sendMessage(std::make_shared<networkMessage>());
        } else { // If we are leader -> broadcast coordinator message instead of heartbeat message. Allows new clients to find coordinator without starting election
            auto msg = std::make_shared<networkMessage>();
            msg->type = static_cast<int>(MessageType_t::COORDINATOR);
            sprintf(msg->data, "%s:%d:%d", getIdentity().data(), getHost().second, election::getUptime());
            sendMessage(msg);
        }
    }
    nextHeartbeat.async_wait(boost::bind(&RemoteConnection::doHeartbeat, this));
}
void RemoteConnection::resetHeartbeat() {
    nextHeartbeat.expires_from_now(boost::posix_time::seconds(HEARTBEAT_INTERVAL));
}
void RemoteConnection::handleTimeout() {
    if(isEphemeral){
        hosts.erase(client->remoteHost);
        connections.erase(client->remoteHost);
    }
}
void RemoteConnection::receive() {
    rx_socket->async_receive_from(boost::asio::buffer(rx_buffer),
                              rx_endpoint,
                              boost::bind(&RemoteConnection::handle_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}
void RemoteConnection::handle_receive(const boost::system::error_code &error, size_t bytes_transferred) {
    if (error) {
        BOOST_LOG_SEV(Logger::getLogger(), severity_levels::error)<<"Connections: Receive failed: " << error.message()<<std::endl;
        return;
    }
    BOOST_LOG_SEV(Logger::getLogger(), trace)<<"Connections: received message from remote" << rx_endpoint.address().to_string() <<std::endl;
    if(bytes_transferred==MSG_SZ){
        const networkMessage* msg = (const networkMessage*)(rx_buffer.data());
        auto msgPtr = networkMessage::mk_shared_copy(msg);
        auto iter = std::find_if(endpointMap.begin(), endpointMap.end(), [this](std::pair<udp::endpoint,std::shared_ptr<Client>> x)->bool { return this->rx_endpoint.address() ==  x.first.address();});
        if(iter != endpointMap.end()) {
            iter->second->handle_receive(msgPtr);
        } else{
            if(rx_endpoint.address().to_string() == std::string("127.0.0.1")){
                auto iter = std::find_if(endpointMap.begin(), endpointMap.end(), [](std::pair<udp::endpoint,std::shared_ptr<Client>> x)->bool { return getHost() ==  x.second->remoteHost;});
                iter->second->handle_receive(msgPtr);
            } else{
                BOOST_LOG_SEV(Logger::getLogger(), trace)<<"Connections: received message from unknown remote" << rx_endpoint.address().to_string()  <<std::endl;
            }
        }
    }
    receive();
}


boost::asio::ip::udp::socket* RemoteConnection::rx_socket = nullptr;
boost::asio::io_service* RemoteConnection::io_service = nullptr;
bool RemoteConnection::initialised = false;
std::set<std::pair<boost::asio::ip::udp::endpoint, std::shared_ptr<Client>>> RemoteConnection::endpointMap;