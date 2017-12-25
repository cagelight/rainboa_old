#include "api_internal.hh"

namespace rainboa::api {

	// ================================
	// ACCT_CREATE -- create a new account
	// ================================
	static aeon::object acct_create(aeon::object const &, cmd_persist & pers) {
		postgres::result res = pers.dbv.exec_params("INSERT INTO account.base (seed) VALUES ($1::BIGINT) RETURNING id", {std::to_string(util::randomized<postgres::bigint_t>())});
		if (!res.tuples_ok()) sqlerror;
		pers.acct_id = res.get_value(0, 0);
		std::string token_name = util::random_str(64, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
		std::string token_hash = util::hash_blake2b(token_name).hex();
		res = pers.dbv.exec_params("INSERT INTO account.token (acct_id, hash) VALUES ($1::BIGINT, $2::TEXT)", {std::to_string(pers.acct_id), token_hash});
		if (!res.cmd_ok()) sqlerror;
		aeon::object ret = begin_api_return(code::success);
		ret["token"] = token_name;
		return ret;
	}
	
	// ================================
	// ACCT_TOKEN -- redeem a user id from a token
	// ================================
	static aeon::object acct_token(aeon::object const & in, cmd_persist & pers) {
		std::string token_name = in["token"];
		std::string token_hash = util::hash_blake2b(token_name).hex();
		postgres::result res = pers.dbv.exec_params("UPDATE account.token SET last_use = NOW() WHERE hash = $1::TEXT RETURNING acct_id", {token_hash});
		if (!res.tuples_ok()) sqlerror;
		if (res.num_rows() != 1) {
			aeon::object ret = begin_api_return(code::invalid_operation);
			debugmsg("token not found");
			return ret;
		}
		pers.acct_id = res.get_value(0, 0);
		aeon::object ret = begin_api_return(code::success);
		ret["acct_id"] = pers.acct_id;
		return ret;
	}
	
	// ================================
	// ACCT_CLAIM -- claim an anonymous account
	// ================================
	static aeon::object acct_claim(aeon::object const & in, cmd_persist & pers) {
		if (!pers.acct_id) {
			aeon::object ret = begin_api_return(code::authorization_required);
			debugmsg("not authorized, nothing to claim");
			return ret;
		}
		postgres::result res = pers.dbv.exec_params("SELECT acct_id FROM account.auth WHERE acct_id = $1::BIGINT", {std::to_string(pers.acct_id)});
		if (!res.tuples_ok()) sqlerror;
		if (res.num_rows() != 0) {
			aeon::object ret = begin_api_return(code::invalid_operation);
			debugmsg("this account has already been claimed");
			return ret;
		}
		std::string username = in["username"].string();
		std::string password = in["password"].string();
		if (!username.size()) {
			aeon::object ret = begin_api_return(code::missing_field);
			debugmsg("username required");
			return ret;
		}
		if (!password.size()) {
			aeon::object ret = begin_api_return(code::missing_field);
			debugmsg("password required");
			return ret;
		}
		postgres::bigint_t salt = util::randomized<postgres::bigint_t>();
		std::string passhash = util::hash_blake2b(password + std::to_string(salt)).hex();
		res = pers.dbv.exec_params("INSERT INTO account.auth (acct_id, username, passhash, salt) VALUES ($1::BIGINT, $2::TEXT, $3::TEXT, $4::BIGINT)", {std::to_string(pers.acct_id), username, passhash, std::to_string(salt)});
		if (!res.cmd_ok()) sqlerror;
		return begin_api_return(code::success);
	}
	
	// ================================
	// ACCT_AUTH -- login to an account
	// ================================
	static aeon::object acct_auth(aeon::object const & in, cmd_persist & pers) {
		std::string username = in["username"].string();
		std::string password = in["password"].string();
		if (!username.size()) {
			aeon::object ret = begin_api_return(code::missing_field);
			debugmsg("username required");
			return ret;
		}
		if (!password.size()) {
			aeon::object ret = begin_api_return(code::missing_field);
			debugmsg("password required");
			return ret;
		}
		postgres::result res = pers.dbv.exec_params("SELECT acct_id, passhash, salt FROM account.auth WHERE username = $1::TEXT", {username});
		if (!res.tuples_ok()) sqlerror;
		if (!res.num_fields()) {
			aeon::object ret = begin_api_return(code::invalid_operation);
			debugmsg("unrecognized username");
			return ret;
		}
		std::string passhash = util::hash_blake2b(password + std::string(res(0, 2))).hex();
		if (passhash != res(0, 1).string()) {
			aeon::object ret = begin_api_return(code::invalid_operation);
			debugmsg("incorrect password");
			return ret;
		}
		pers.acct_id = res(0, 0);
		std::string token_name = util::random_str(64, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
		std::string token_hash = util::hash_blake2b(token_name).hex();
		pers.dbv.cmd_params("UPDATE account.auth SET last_login = NOW() WHERE acct_id = $1::BIGINT", {std::to_string(pers.acct_id)});
		res = pers.dbv.exec_params("INSERT INTO account.token (acct_id, hash) VALUES ($1::BIGINT, $2::TEXT)", {std::to_string(pers.acct_id), token_hash});
		if (!res.cmd_ok()) sqlerror;
		aeon::object ret = begin_api_return(code::success);
		ret["token"] = token_name;
		return ret;
	}
	
	void auth_init(postgres::pool::conview & dbv) {
		if (!dbv.cmd(R"(
			CREATE SCHEMA IF NOT EXISTS account
		)")) throwe(startup);
		
		// BASE
		if (!dbv.cmd(R"(
			CREATE TABLE IF NOT EXISTS account.base (
				id BIGSERIAL PRIMARY KEY,
				seed BIGINT NOT NULL,
				create_date TIMESTAMP NOT NULL DEFAULT NOW()
			)
		)")) throwe(startup);
		
		// AUTH
		if (!dbv.cmd(R"(
			CREATE TABLE IF NOT EXISTS account.auth (
				acct_id BIGINT REFERENCES account.base(id) NOT NULL UNIQUE,
				username VARCHAR(64) NOT NULL UNIQUE,
				passhash CHAR(128) NOT NULL,
				salt BIGINT NOT NULL,
				last_login TIMESTAMP
			)
		)")) throwe(startup);
		
		// TOKEN
		if (!dbv.cmd(R"(
			CREATE TABLE IF NOT EXISTS account.token (
				acct_id BIGINT REFERENCES account.base(id) NOT NULL,
				hash CHAR(128) NOT NULL UNIQUE,
				last_use TIMESTAMP
				
			)
		)")) throwe(startup);
		if (!dbv.cmd(R"(
			CREATE INDEX IF NOT EXISTS token_acct_id_idx
			ON account.token(acct_id)
		)")) throwe(startup);
		
		register_cmd("acct_create", acct_create);
		register_cmd("acct_token", acct_token);
		register_cmd("acct_claim", acct_claim);
		register_cmd("acct_auth", acct_auth);
	}
}
