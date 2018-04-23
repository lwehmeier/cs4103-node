//
// Created by lw96 on 09/04/18.
//

#include "logging.h"
#include "networkParser.h"
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

void init_builtin_syslog(std::string syslogSrv){
        boost::shared_ptr< sinks::synchronous_sink< sinks::syslog_backend > > sink(
                new sinks::synchronous_sink< sinks::syslog_backend >());

        std::string formatStr = getIdentity() + std::string(" : %1%");
        sink->set_formatter
                (
                        expr::format(formatStr)
                        % expr::smessage
                );

        sinks::syslog::custom_severity_mapping< severity_levels > mapping("Severity");
        mapping[trace] = sinks::syslog::debug;
        mapping[debug] = sinks::syslog::debug;
        mapping[info] = sinks::syslog::info;
        mapping[warning] = sinks::syslog::warning;
        mapping[error] = sinks::syslog::critical;

        sink->locked_backend()->set_severity_mapper(mapping);
        sink->locked_backend()->set_target_address(syslogSrv, 9898);
        logging::core::get()->add_sink(sink);

        typedef sinks::synchronous_sink< sinks::text_ostream_backend > text_sink;
        boost::shared_ptr< text_sink > text_backend = boost::make_shared< text_sink >();

        text_backend->locked_backend()->add_stream(
                boost::shared_ptr<std::ostream>(&std::cout, boost::null_deleter()));
        text_backend->locked_backend()->auto_flush(true);
        logging::add_common_attributes();
        text_backend->set_formatter
                (
                        expr::stream
                                << '['
                                << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S")
                                << "] - "
                                << expr::smessage
                );
        text_backend->set_filter(boost::log::expressions::attr<severity_levels>("Severity") >= debug );
        logging::core::get()->add_sink(text_backend);
    }
    Logger Logger::instance;