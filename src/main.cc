#include "api.hh"

namespace aeon = asterid::aeon;

struct rainboa_exchange : public locust::basic_responder {
	virtual void respond(locust::basic_exchange_interface & bei) override {
		
		if (bei.req_head.method == "OPTIONS") {
			bei.res_head.code = locust::http::status_code::ok;
			std::string origin = bei.req_head.field("Origin");
			if (!origin.empty()) bei.res_head.fields["Access-Control-Allow-Origin"] = origin;
			std::string headers = bei.req_head.field("Access-Control-Request-Headers");
			if (!origin.empty()) bei.res_head.fields["Access-Control-Allow-Headers"] = headers;
			bei.res_head.fields["Access-Control-Allow-Methods"] = "OPTIONS POST";
			return;
		}
		
		bei.res_head.fields["Access-Control-Allow-Origin"] = "*";
		
		if (bei.req_head.method != "POST") {
			bei.res_head.code = locust::http::status_code::method_not_allowed;
			bei.res_head.fields["Content-Type"] = "text/plain";
			bei.res_body << u8"A weird snake bites you and deals ∞ physical and ∞ poison damage, you die. 🐍";
			return;
		}
		
		if (!bei.req_head.content_length()) {
			bei.res_head.code = locust::http::status_code::bad_request;
			bei.res_head.fields["Content-Type"] = "text/plain; charset=UTF-8";
			bei.res_body << u8"It's empty... there's nothing here... what are you playing at? 🐍";
			return;
		}
		
		if (bei.req_head.content_type() != "application/json" && bei.req_head.content_type() != "application/aeon") {
			bei.res_head.code = locust::http::status_code::unsupported_media_type;
			bei.res_head.fields["Content-Type"] = "text/plain; charset=UTF-8";
			bei.res_body << u8"The heck is this? I can't eat this. 🐍";
			return;
		}
		
		aeon::object rec {};
		bool return_aeon = false;
		
		try {
			if (bei.req_head.content_type() == "application/aeon") {
				asterid::buffer_assembly body {bei.req_body};
				rec = aeon::object::parse_binary(body);
				return_aeon = true;
			} else {
				rec = aeon::object::parse_text(bei.req_body.to_string());
			}
		} catch (aeon::exception::parse &) {
			bei.res_head.code = locust::http::status_code::bad_request;
			bei.res_head.fields["Content-Type"] = "text/plain; charset=UTF-8";
			bei.res_body << u8"You got wax in your internet tubes? 🐍";
			return;
		}
		
		aeon::object ret = rainboa::api::process(rec);
		
		bei.res_head.code = locust::http::status_code::ok;
		if (return_aeon) {
			bei.res_head.fields["Content-Type"] = "application/aeon";
			ret.serialize_binary(bei.res_body);
		} else {
			bei.res_head.fields["Content-Type"] = "application/json";
			bei.res_body << ret.serialize_text();
		}
	}
};

static std::atomic_bool run_sem {true};

static void handle_signal(int sig) {
	switch (sig) {
		default:
			return;
		case SIGINT:
			if (run_sem) run_sem.store(false);
			else std::terminate();
	}
}

int main() {	
	signal(SIGINT, handle_signal);
	rainboa::util::init();
	try {
		rainboa::api::init();
		asterid::cicada::server sv {8081, false, NUM_CON};
		sv.begin<locust::http::protocol<locust::basic_exchange<rainboa_exchange>>>();
		sv.master( [](){return run_sem.load();} );
	} catche(startup) {
		scilogvf << "startup exception occurred, cannot continue";
	} catchall {
		scilogvf << "unknown exception occurred, cannot continue";
	}
	rainboa::api::term();
	rainboa::util::term();
}
