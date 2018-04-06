//
// Created by lw96 on 06/04/18.
//

#ifndef CS4103_NETWORKMESSAGE_H
#define CS4103_NETWORKMESSAGE_H
struct networkMessage{
    int type;
    int payload;
    char data[1024];
};
#endif //CS4103_NETWORKMESSAGE_H
