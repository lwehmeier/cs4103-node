//
// Created by lw96 on 22/04/18.
//
#include "main.h"
#include "visitorAccessMgr.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/VariadicBind.h>
#include "logging.h"

void VisitorAccessManager::setup(std::string db_path) {
    BOOST_LOG_SEV(Logger::getLogger(), info)<<"Initialising visitor database connector for database: " << db_path  <<std::endl;
    database_path = db_path;
}
bool VisitorAccessManager::init_leader(int permittedVisitors){
    BOOST_LOG_SEV(Logger::getLogger(), debug)<<"Checking and preparing database after leader change: " << database_path <<std::endl;
    SQLite::Database db(database_path, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE);
    db.exec("DROP TABLE IF EXISTS cfg");
    db.exec("CREATE TABLE cfg (id INTEGER PRIMARY KEY, value INTEGER)");
    SQLite::Statement   query(db, "INSERT INTO cfg VALUES (0, ?)");
    query.bind(1, permittedVisitors);
    query.exec();
    if(!db.tableExists("visitors")) {
        db.exec("CREATE TABLE visitors (ticket_id INTEGER PRIMARY KEY)");
    }
}
bool VisitorAccessManager::add_visitor (long ticket)
{
    try
    {
        SQLite::Database    db(database_path, SQLite::OPEN_READWRITE);
        SQLite::Statement   query(db, "INSERT INTO visitors values (?)");
        query.bind(1, ticket);
        int permittedVisitors = db.execAndGet("SELECT value FROM cfg WHERE id=0");
        int currentVisitors = db.execAndGet("SELECT count(ticket_id) FROM visitors");
        if(currentVisitors < permittedVisitors) {
            return query.exec();
        }
        return false;
    }
    catch (std::exception& e)
    {
        BOOST_LOG_SEV(Logger::getLogger(), error)<<"SQLite exception: " << e.what()  <<std::endl;
        return false;
    }
}
bool VisitorAccessManager::remove_visitor (int ticket)
{
    try
    {
        SQLite::Database    db(database_path, SQLite::OPEN_READWRITE);
        SQLite::Statement   query(db, "DELETE FROM visitors WHERE ticket_id = ?");
        query.bind(1, ticket);
        return query.exec();
    }
    catch (std::exception& e)
    {
        BOOST_LOG_SEV(Logger::getLogger(), error)<<"SQLite exception: " << e.what()  <<std::endl;
        return false;
    }
}
std::string VisitorAccessManager::getCurrentVisitors() {
    std::string s="";
    try
    {
        SQLite::Database    db(database_path);
        SQLite::Statement   query(db, "SELECT * FROM visitors");
        while(query.executeStep()){
            s+= std::to_string(query.getColumn(0).getInt())+ "   ";
        }
    }
    catch (std::exception& e)
    {
        BOOST_LOG_SEV(Logger::getLogger(), error)<<"SQLite exception: " << e.what()  <<std::endl;
        return "INTERNAL DATABASE ERROR\r\n";
    }
    return s+"\r\r";
}
std::string VisitorAccessManager::database_path("/cs/home/lw96/cs4103.db3");