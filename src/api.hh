#pragma once
#include "util.hh"
#include "psql.hh"

#include <asterid/aeon.hh>

namespace rainboa::api {
	
	void init();
	void term();
	
	asterid::aeon::object process(asterid::aeon::object const & req);
	
}
