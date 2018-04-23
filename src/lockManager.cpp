//
// Created by leon on 11/04/18.
//

#include "lockManager.h"
#include "main.h"
#include "networkMessage.h"
#include "election.h"

//TODO: timeout early so we cannot get conflicts if leader fails

bool lockManager::getLock(lockManager_callback_t cb){
    if(!election::election_leader || election::electionActive){
        BOOST_LOG_SEV(Logger::getLogger(), info)<<"LockManager: The system currently has no leader, could not request lock."<<std::endl;
        return false; //we currently don't have a leader
    }
    lockCallbacks.push_back(cb);
    if(lockCallbacks.size()>1){//already waiting for lock, queue request
        return true;
    }
    auto msg = std::make_shared<networkMessage>();
    msg->type= static_cast<int>(MessageType_t::LOCK);
    connections[*election::election_leader]->sendMessage(msg);
    connections[*election::election_leader]->registerCallback(lockManager::lockAcquired, MessageType_t::GRANT);
    return true;
}
void lockManager::leaderChanged(const std::pair<std::string, int>& remote) {
    cancel();
    currentLockHolder.second=0;
    if(election::isCurrentLeader()){//we are leader
        for(auto [host, con ]: connections) {
            connections[host]->registerCallback(lockManager::handleLock, MessageType_t::LOCK);
            connections[host]->registerCallback(lockManager::handleUnlock, MessageType_t::UNLOCK);
        }
    } else{
        for(auto host : hosts) {
            connections[host]->unregisterCallback(MessageType_t::LOCK);
            connections[host]->unregisterCallback(MessageType_t::UNLOCK);
        }
        connections[getHost()]->unregisterCallback(MessageType_t::LOCK);
        connections[getHost()]->unregisterCallback(MessageType_t::UNLOCK);
    }
}
void lockManager::cancel(){
    lockCallbacks.clear();
    unlock();
}
void lockManager::unlock(){
    connections[*election::election_leader]->unregisterCallback(MessageType_t::GRANT);
    if(!election::election_leader || election::electionActive){
        return; //we currently don't have a leader
    }
    BOOST_LOG_SEV(Logger::getLogger(), debug)<<"LockManager: releasing lock"<<std::endl;
    auto msg = std::make_shared<networkMessage>();
    msg->type= static_cast<int>(MessageType_t::UNLOCK);
    connections[*election::election_leader]->sendMessage(msg);
}
void lockManager::lockAcquired(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote){
    if(!node_equals(remote, *election::election_leader)){//received grant from non leader node
        BOOST_LOG_SEV(Logger::getLogger(), debug)<<"LockManager: received grant from non-master node "<<remote.first<<":"<<remote.second<<std::endl;
        return;
    }
    BOOST_LOG_SEV(Logger::getLogger(), debug)<<"LockManager: acquired lock"<<std::endl;
    for(auto f : lockCallbacks){
        f();
    }
    lockCallbacks.clear();
}
void lockManager::grantLock(const std::pair<std::string, int>& remote){
    currentLockHolder = remote;
    auto msg = std::make_shared<networkMessage>();
    msg->type= static_cast<int>(MessageType_t::GRANT);
    connections[currentLockHolder]->sendMessage(msg);
    lock_timeout->cancel();
    lock_timeout->expires_from_now(boost::posix_time::seconds(LOCK_DURATION_S));
    lock_timeout->async_wait(boost::bind(&lockManager::expire_lock));
}
void lockManager::expire_lock(){
    if (lock_timeout->expires_at() <= boost::asio::deadline_timer::traits_type::now()) {//if expired
        handleUnlock(nullptr, currentLockHolder);
    }
}
void lockManager::handleLock(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote){
    BOOST_LOG_SEV(Logger::getLogger(), debug)<<"LockManager: received lock req from node "<<remote.first<<":"<<remote.second<<std::endl;
    if(currentLockHolder.second==0){
        grantLock(remote);
        return;
    }
    auto iter = std::find(lockRequests.begin(), lockRequests.end(), remote);
    if(iter == lockRequests.end()){
        lockRequests.push_back(remote);
    }
}
void lockManager::handleUnlock(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int> &remote) {
    BOOST_LOG_SEV(Logger::getLogger(), debug)<<"LockManager: received unlock req from node "<<remote.first<<":"<<remote.second<<std::endl;
    if(remote == currentLockHolder){
        currentLockHolder.second=0;
        if(lockRequests.size()>0){
            grantLock(lockRequests.front());
            lockRequests.erase(lockRequests.begin());
        }
        return;
    }
    BOOST_LOG_SEV(Logger::getLogger(), debug)<<"LockManager: current lock holder != unlocker "<<std::endl;
    auto iter = std::find(lockRequests.begin(), lockRequests.end(), remote);
    if(iter != lockRequests.end()){
        lockRequests.erase(iter);
    }
}
void lockManager::setup(boost::asio::io_service &ios) {
    lock_timeout = new boost::asio::deadline_timer(ios);
}

std::vector<lockManager_callback_t> lockManager::lockCallbacks;
std::pair<std::string, int> lockManager::currentLeader;
std::vector<std::pair<std::string, int>> lockManager::lockRequests;
std::pair<std::string, int> lockManager::currentLockHolder;
boost::asio::deadline_timer* lockManager::lock_timeout;