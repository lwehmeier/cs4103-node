//
// Created by lw96 on 06/04/18.
//

#ifndef CS4103_NETWORKMESSAGE_H
#define CS4103_NETWORKMESSAGE_H

#define MSG_SZ 1036
#include <memory>

enum class MessageType_t { HEARTBEAT, TEXT, ELECTION, ACK, COORDINATOR, LOCK, UNLOCK, GRANT };

struct networkMessage{
    int type= static_cast<int>(MessageType_t ::HEARTBEAT);
    int sequence;
    int payload=0;
    char data[1024];
    static std::shared_ptr<networkMessage> mk_shared_copy(const networkMessage* msg);
};
#endif //CS4103_NETWORKMESSAGE_H
