#include "connectionManager.h"
#include "logging.h"
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
    //std::cout << "Sent Payload --- " << sent << "\n";
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
            //std::cout<<"Received heartbeat from "<<remoteName << std::endl;
        } else {
            //std::cout<<"Received message from "<<remoteName << std::endl;
            queue.push_back(msg);
        }
    }
    alive = true;
    wait();
}

    void Client::wait() {
        // Set a deadline for the asynchronous operation.
        deadline_.expires_from_now(boost::posix_time::seconds(HEARTBEAT_TIMEOUT));
    }

    void Client::Receiver(){
        wait();
    }

void Client::check_deadline()
{
    // Check whether the deadline has passed. We compare the deadline against
    // the current time since a new asynchronous operation may have moved the
    // deadline before this actor had a chance to run.
    if (deadline_.expires_at() <= deadline_timer::traits_type::now() && socket->is_open())
    {
        std::cout<<"deadline expired for remote "<<remoteName<<std::endl;
        src::severity_logger< severity_levels > lg(keywords::severity = normal);
        BOOST_LOG_SEV(lg, warning) << "Connection to node timed out: " << remoteName;
        alive = false;
        timeout_cb(remoteHost);
        // The deadline has passed. The outstanding asynchronous operation needs
        // to be cancelled so that the blocked receive() function will return.
        //
        // Please note that cancel() has portability issues on some versions of
        // Microsoft Windows, and it may be necessary to use close() instead.
        // Consult the documentation for cancel() for further information.
        //socket.cancel();

        // There is no longer an active deadline. The expiry is set to positive
        // infinity so that the actor takes no action until a new deadline is set.
        deadline_.expires_at(boost::posix_time::pos_infin);
    }
    // Put the actor back to sleep.
    deadline_.async_wait(boost::bind(&Client::check_deadline, this));
}


void RemoteConnection::enableHeartbeat() {
    resetHeartbeat();
    nextHeartbeat.async_wait(boost::bind(&RemoteConnection::doHeartbeat, this));
}
void RemoteConnection::doHeartbeat() {
    if (nextHeartbeat.expires_at() <= deadline_timer::traits_type::now()){
        resetHeartbeat();
        //std::cout<<"Sending heartbeat to "<<remoteName<<std::endl;
        sendMessage(std::make_shared<networkMessage>());
    }
    nextHeartbeat.async_wait(boost::bind(&RemoteConnection::doHeartbeat, this));
}
void RemoteConnection::resetHeartbeat() {
    nextHeartbeat.expires_from_now(boost::posix_time::seconds(HEARTBEAT_INTERVAL));
}
void RemoteConnection::receive() {
    rx_socket->async_receive_from(boost::asio::buffer(rx_buffer),
                              rx_endpoint,
                              boost::bind(&RemoteConnection::handle_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}
void RemoteConnection::handle_receive(const boost::system::error_code &error, size_t bytes_transferred) {
    if (error) {
        std::cout << "Receive failed: " << error.message() << "\n";
        return;
    }
    //std::cout << "Received: '" << "$data" << "' (" << error.message() << ")\n";
    if(bytes_transferred==MSG_SZ){
        const networkMessage* msg = (const networkMessage*)(rx_buffer.data());
        //std::cout<<"parsed message"<<std::endl;
        auto msgPtr = networkMessage::mk_shared_copy(msg);
        auto iter = std::find_if(endpointMap.begin(), endpointMap.end(), [this](std::pair<udp::endpoint,std::shared_ptr<Client>> x)->bool { return this->rx_endpoint.address() ==  x.first.address();});
        if(iter != endpointMap.end()) {
            iter->second->handle_receive(msgPtr);
        } else{
            std::cerr << "received message from unknown remote" << std::endl;
        }
    }
    receive();
}


boost::asio::ip::udp::socket* RemoteConnection::rx_socket = nullptr;
boost::asio::io_service* RemoteConnection::io_service = nullptr;
bool RemoteConnection::initialised = false;
std::set<std::pair<boost::asio::ip::udp::endpoint, std::shared_ptr<Client>>> RemoteConnection::endpointMap;