//
// Created by leon on 11/04/18.
//

#ifndef CS4103_LOCKMANAGER_H
#define CS4103_LOCKMANAGER_H
#include <unordered_map>
#include <functional>
#include <vector>
#include <memory>
#include "networkMessage.h"
#include "main.h"
#define LOCK_DURATION_S 2
typedef std::function<void()> lockManager_callback_t;
struct lockManager{
    static bool getLock(lockManager_callback_t cb);
    static void cancel();
    static void unlock();
    static void leaderChanged(const std::pair<std::string, int>& remote);
    static void setup(boost::asio::io_service& ios);
private:
    static void lockAcquired(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote);
    static void handleLock(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote);
    static void handleUnlock(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote);
    static void grantLock(const std::pair<std::string, int>& remote);
    static void expire_lock();
    static std::vector<lockManager_callback_t> lockCallbacks;
    static std::vector<std::pair<std::string, int>> lockRequests;
    static std::pair<std::string, int> currentLockHolder;
    static std::pair<std::string, int> currentLeader;
    static boost::asio::deadline_timer *lock_timeout;
};
#endif //CS4103_LOCKMANAGER_H
