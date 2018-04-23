//
// Created by lw96 on 22/04/18.
//

#ifndef CS4103_VISITORACCESSMGR_H
#define CS4103_VISITORACCESSMGR_H
#include <string>
struct VisitorAccessManager {
    static bool init_leader(int permittedVisitors);

    static bool add_visitor(long ticket);

    static bool remove_visitor(int ticket);

    static std::string getCurrentVisitors();

    static std::string database_path;
};
#endif //CS4103_VISITORACCESSMGR_H
