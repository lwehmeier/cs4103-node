//
// Created by lw96 on 06/04/18.
//

#ifndef CS4103_NETWORKPARSER_H
#define CS4103_NETWORKPARSER_H
#include <string>
#include <vector>
#include <set>
void readGraph();
std::string getIdentity();
std::set<std::pair<std::string, int>> getNeighbourHosts();
std::pair<std::string, int> getHost();
std::set<std::pair<std::string, int>> getAllHosts();
#endif //CS4103_NETWORKPARSER_H
