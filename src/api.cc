#include "api_internal.hh"
#include "psql.hh"

static std::unique_ptr<postgres::pool> pgpool;

#define debugmsg(msg) if (pers.debug_mode) ret["debug"] = msg
#define sqlerror do {aeon::object ret = begin_api_return(code::database_error); debugmsg(res.get_error()); scilogvs << res.get_error(); return ret;} while (0)

namespace rainboa::api {
	
	void init() {
		pgpool.reset(new postgres::pool {"rainboa", NUM_CON});
		if (!pgpool->ok()) {
			scilogve << "failed to create database connection pool";
			throwe(startup);
		}
		auto dbv = pgpool->acquire();
		
		auth_init(dbv);
	}
	
	void term() {
		pgpool.reset();
	}

	typedef std::unordered_map<aeon::str_t, api_f> map_t;
	
	aeon::object begin_api_return(code c) {
		aeon::object ret = aeon::map();
		ret["err"] = static_cast<aeon::int_t>(c);
		return ret;
	}
	
	static aeon::object debug(aeon::object const &, cmd_persist & pers) {
		pers.debug_mode = true;
		return begin_api_return(code::success);
	}
	
	static map_t function_map {
		{"debug", debug}
	};
	
	void register_cmd(std::string const & cmd, api_f func) {
		function_map[cmd] = func;
	}
	
	aeon::object process(aeon::object const & rec) {
		cmd_persist cmdp = {
			false,
			0,
			pgpool->acquire()
		};
		
		aeon::object ret = aeon::array();
		aeon::ary_t & ret_ary = ret.array();
		for (aeon::object const & obj : rec.array()) {
			if (!obj.is_map()) { ret_ary.push_back(aeon::null); continue; }
			auto func_i = function_map.find(obj["cmd"].string());
			ret_ary.push_back( func_i == function_map.end() ? begin_api_return(code::unknown_cmd) : func_i->second(obj, cmdp) );
		}
		return ret;
	}
}


