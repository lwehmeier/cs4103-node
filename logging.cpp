//
// Created by lw96 on 09/04/18.
//

#include "logging.h"
#include "networkParser.h"
void init_builtin_syslog()
    {        // Create a syslog sink
        boost::shared_ptr< sinks::synchronous_sink< sinks::syslog_backend > > sink(
                new sinks::synchronous_sink< sinks::syslog_backend >());

        std::string formatStr = getIdentity() + std::string(" : %1%: %2%");
        sink->set_formatter
                (
                        expr::format(formatStr)
                        % expr::attr< unsigned int >("RecordID")
                        % expr::smessage
                );

        // We'll have to map our custom levels to the syslog levels
        sinks::syslog::custom_severity_mapping< severity_levels > mapping("Severity");
        mapping[normal] = sinks::syslog::info;
        mapping[warning] = sinks::syslog::warning;
        mapping[error] = sinks::syslog::critical;

        sink->locked_backend()->set_severity_mapper(mapping);

        // Set the remote address to sent syslog messages to
        sink->locked_backend()->set_target_address("pc2-002-l", 9898);

        // Add the sink to the core
        logging::core::get()->add_sink(sink);

        // Add some attributes too
        logging::core::get()->add_global_attribute("RecordID", attrs::counter< unsigned int >());

    }