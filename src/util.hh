#pragma once

#include <asterid/strops.hh>
#include <asterid/buffer_assembly.hh>

#include <locust/locust.hh>

#include <csignal>
#include <sstream>
#include <functional>
#include <unordered_map>



#define NUM_CON 4

namespace rainboa {
	
	namespace exception {
		struct startup {};
	}
	
	namespace util {
		
		void init();
		void term() noexcept;
		
		enum struct log_level {
			info,
			warning,
			error,
			fatal,
			debug,
			sql
		};
		void log(log_level, std::string const &);
		
		namespace logging_internal {
			struct cilogger {
				util::log_level level;
				std::string str;
				inline cilogger(util::log_level lev, std::string const & init = "") : level(lev), str(init) {}
				inline ~cilogger() { util::log(level, str); }
				
				inline cilogger & operator << ( std::string const & other ) { str += other; return *this; }
				inline cilogger & operator << ( char const * other ) { str += other; return *this; }
				template <typename T> inline cilogger & operator << ( T const & other ) { str += std::to_string(other); return *this; }
			};
			
			struct cilogger_dummy {
				inline cilogger_dummy() {}
				template <typename T> inline cilogger_dummy & operator << ( T const & ) {return *this; }
			};
		}
		
		asterid::buffer_assembly random(size_t len);
		std::string random_str(size_t len, std::string const & chars);
		asterid::buffer_assembly hash_blake2b(std::string str);
		
		void randomize_data(void * ptr, size_t len);
		template <typename T> void randomize(T & v) { randomize_data(reinterpret_cast<void *>(&v), sizeof(T)); }
		template <typename T> T randomized() { T v; randomize<T>(v); return v; }
	}
}

#define scilog(prefix, lev) rainboa::util::logging_internal::cilogger { lev, #prefix ": " }
#define scilogv(prefix, lev) rainboa::util::logging_internal::cilogger { lev, asterid::strf("%s (%s): ", #prefix, _as_here) }
#define scilogi scilog(INFO, rainboa::util::log_level::info)
#define scilogvi scilogv(INFO, rainboa::util::log_level::info)
#define scilogw scilog(WARNING, rainboa::util::log_level::warning)
#define scilogvw scilogv(WARNING, rainboa::util::log_level::warning)
#define sciloge scilog(ERROR, rainboa::util::log_level::error)
#define scilogve scilogv(ERROR, rainboa::util::log_level::error)
#define scilogf scilog(FATAL, rainboa::util::log_level::fatal)
#define scilogvf scilogv(FATAL, rainboa::util::log_level::fatal)
#define scilogs scilog(SQL ERROR, rainboa::util::log_level::sql)
#define scilogvs scilogv(SQL ERROR, rainboa::util::log_level::sql)
#ifdef RAINBOA_DEBUG
	#define scilogb scilog(DEBUG, rainboa::util::log_level::debug)
	#define scilogvb scilogv(DEBUG, rainboa::util::log_level::debug)
#else
	#define scilogb rainboa::util::logging_internal::cilogger_dummy {}
	#define scilogvb rainboa::util::logging_internal::cilogger_dummy {}
#endif

#define throwe(type) throw rainboa::exception::type {}
#define catche(type) catch (rainboa::exception::type)
#define catchall catch(...)
