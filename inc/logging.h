//
// Created by lw96 on 09/04/18.
//

#ifndef CS4103_LOGGING_H
#define CS4103_LOGGING_H
#include <stdexcept>
#include <string>
#include <iostream>
#include <boost/shared_ptr.hpp>

#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/syslog_backend.hpp>

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

enum severity_levels
{
    trace,
    debug,
    info,
    warning,
    error
};
void init_builtin_syslog(std::string syslogSrv);

class Logger{
public:
    static src::severity_logger< severity_levels >& getLogger(){
        return instance.boost_lg;
    }
    static void setup(std::string syslogSrv){
        init_builtin_syslog(syslogSrv);
    }
private:
    static Logger instance;
    src::severity_logger< severity_levels > boost_lg;
    Logger(){
        boost_lg = src::severity_logger< severity_levels >(keywords::severity = info);
        BOOST_LOG_SEV(boost_lg, info) << "Logging: Log Service started" << std::endl;
    };
};
#endif //CS4103_LOGGING_H
