//
// Created by lw96 on 06/04/18.
//
#include <memory>
#include <cstring>
#include "networkMessage.h"

std::shared_ptr<networkMessage> networkMessage::mk_shared_copy(const networkMessage* msg){
    auto ret = std::make_shared<networkMessage>();
    std::memcpy(&(ret->type),&(msg->type),MSG_SZ);
    return ret;
}