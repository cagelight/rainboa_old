#pragma once
#include "api.hh"
#include "psql.hh"

#define debugmsg(msg) if (pers.debug_mode) ret["debug"] = msg
#define sqlerror do {aeon::object ret = begin_api_return(code::database_error); debugmsg(res.get_error()); scilogvs << res.get_error(); return ret;} while (0)

namespace aeon = asterid::aeon;

namespace rainboa::api {
	
	struct cmd_persist {
		bool debug_mode;
		postgres::bigint_t acct_id;
		postgres::pool::conview dbv;
	};

	enum struct code : aeon::int_t {
		success,
		not_implemented,
		unknown_cmd,
		missing_field,
		invalid_operation,
		database_error,
		authorization_required,
	};
	
	typedef std::function<aeon::object(aeon::object const &, cmd_persist &)> api_f;
	
	aeon::object begin_api_return(code);
	void register_cmd(std::string const & cmd, api_f);
	
	void auth_init(postgres::pool::conview & dbv);
	
}
