//
// Created by leon on 11/04/18.
//

#ifndef CS4103_ELECTION_H
#define CS4103_ELECTION_H

#include <iostream>
#include "main.h"
#include "networkMessage.h"
#include <functional>
using namespace std;

typedef std::function<void(const std::pair<std::string, int>& remote)> leaderchange_callback_t;

class election {
public:
    static const std::pair<string, int> *election_q;
    static const std::pair<string, int> *election_leader;
    static bool election_init;
    static bool electionActive;
    static bool isLeader;
    static std::unordered_map<std::pair<string, int>, std::pair<bool, std::string>, pairhash> election_acks;
    static leaderchange_callback_t leaderchange;

    static void setLeaderChange(leaderchange_callback_t cb);
    static bool isCurrentLeader();
    static std::string getHighestMetric();
    static int getUptime();
    static void startElection();
    static void handleElection(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& parent);
    static void handleAck(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote);
    static void handleCoord(std::shared_ptr<networkMessage> rxMessage, const std::pair<std::string, int>& remote);
    static void handleTimeout(const std::pair<std::string, int>& remote);
    static void createLeaderConIfNeeded();
};


#endif //CS4103_ELECTION_H
