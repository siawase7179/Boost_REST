
#include <boost/core/null_deleter.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/support/date_time.hpp>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/record_ordering.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>


namespace attrs = boost::log::attributes;
namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

using namespace logging::trivial;

#ifndef __FILENAME__
    #define __FILENAME__ (char*)basename(__FILE__)
#endif

#ifndef _TRACE_LOG_
#define _TRACE_LOG_(logger, sev, format, ...) \
    do { \
        char buf[1024 * 10] = {0x00, }; \
        snprintf(buf, sizeof(buf), format, ##__VA_ARGS__); \
        BOOST_LOG_SEV(logger, sev) <<"["<< sev <<"]" << "(" << __FILENAME__ << ":" << __LINE__ << ") " <<(std::string)buf; \
    } while (0)
#endif

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(global_lg, src::severity_logger<severity_level>)

class BoostLogger {
private:
	typedef sinks::text_file_backend file_backend;
    
	typedef sinks::asynchronous_sink<sinks::text_file_backend, sinks::unbounded_ordering_queue
		<logging::attribute_value_ordering<unsigned int, std::less<unsigned int>>>> file_aync_sink_t;
	boost::shared_ptr<file_aync_sink_t> file_sink;

	typedef sinks::text_ostream_backend stdout_sink_t;
	typedef sinks::synchronous_sink<stdout_sink_t> stdout_sync_sink_t;
	boost::shared_ptr<stdout_sink_t> stdout_backend;

	boost::shared_ptr<logging::core> core;    
public: 
	BoostLogger(){
		try {
            core = logging::core::get();

            stdout_backend = boost::make_shared<stdout_sink_t>();
            stdout_backend->add_stream(boost::shared_ptr< std::ostream >(&std::clog, boost::null_deleter()));
            
            stdout_backend->auto_flush(true);

            boost::shared_ptr< stdout_sync_sink_t > stdout_sink(new stdout_sync_sink_t(stdout_backend));
            
            stdout_sink->set_formatter(
                expr::stream
                << "["
                << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                << "]"
                << expr::smessage
            );

            file_sink = boost::make_shared<file_aync_sink_t>(boost::make_shared<file_backend>(), 
                keywords::order = logging::make_attr_ordering("RecordID", std::less<unsigned int>()));
            
            file_sink->locked_backend()->set_file_collector(sinks::file::make_collector(keywords::target = "logs"));
            file_sink->locked_backend()->set_time_based_rotation(sinks::file::rotation_at_time_point(0, 0, 0));
            file_sink->locked_backend()->set_open_mode(std::ios_base::out | std::ios_base::app);
            file_sink->locked_backend()->set_file_name_pattern("logs/boost_srv_%Y%m%d.log");
            file_sink->locked_backend()->scan_for_files();
            file_sink->locked_backend()->auto_flush(true);

            file_sink->set_formatter(
                expr::stream
                << "["
                << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                << "]"
                << expr::smessage
            );

            core->add_sink(file_sink);
            core->add_sink(stdout_sink);

            core->add_global_attribute("TimeStamp", attrs::local_clock());
            core->add_global_attribute("RecordID", attrs::counter< unsigned int >());
            
        }
        catch (std::exception& e){
            std::cout << "FAILURE:" << e.what() << std::endl;
        }
    
	}

	~BoostLogger(){
		core->remove_all_sinks();

		file_sink->stop();
		file_sink->flush();		
		file_sink.reset();
	}
};
