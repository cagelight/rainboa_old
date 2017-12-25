#pragma once
#include "util.hh"

inline std::string & sql_sanitize(std::string & str) {
	std::string::iterator i = str.begin();
	while (i != str.end()) {
		if (*i == '\'') {
			i = str.insert(i, '\'');
			i++;
		}
		i++;
	}
	return str;
}

struct pg_result;

namespace postgres {
	
	typedef int64_t bigint_t;
	typedef int32_t int_t;
	
	struct value {
		inline value(char const * str) : str(str) {}
		std::string const & string() const { return str; }
		int_t integer() const { return strtol(str.c_str(), nullptr, 10); }
		bigint_t biginteger() const { return strtoll(str.c_str(), nullptr, 10); }
		inline operator std::string const & () const { return str; }
		inline operator int_t () const { return integer(); }
		inline operator bigint_t () const { return biginteger(); }
	protected:
		std::string str;
	};

	struct result {
		result();
		result(pg_result *);
		result(result const &) = delete;
		result(result && other);
		~result();
		
		int num_fields() const;
		int num_rows() const;
		value get_value(int row, int field) const;
		std::string get_error() const;
		bool cmd_ok() const;
		bool tuples_ok() const;
		
		result & operator = (result const & other) = delete;
		result & operator = (result && other);
		inline value operator () (int row, int field) const { return get_value(row, field); }
	private:
		struct private_data;
		std::unique_ptr<private_data> data;
	};

	struct connection {
		connection() = delete;
		connection(std::string const & dbname);
		connection(connection const &) = delete;
		connection(connection &&) = delete;
		~connection();
		
		result exec(std::string const & cmd);
		result exec_params(std::string const & cmd, std::initializer_list<std::string_view>);
		inline bool cmd(std::string const & cmd) { result res = exec(cmd); if (res.cmd_ok()) return true; else { scilogs << res.get_error(); return false; } }
		inline bool cmd_params(std::string const & cmd, std::initializer_list<std::string_view> params) { result res = exec_params(cmd, std::move(params)); if (res.cmd_ok()) return true; else { scilogs << res.get_error(); return false; } }
		bool ok();
		
	private:
		struct private_data;
		std::unique_ptr<private_data> data;
	};

	struct pool {
		
		struct pool_con {
			pool_con() = delete;
			pool_con(std::string dbname, std::mutex & cvm, std::condition_variable & cv) : con(dbname), in_use(false), cvm(cvm), cv(cv) {}
			~pool_con() = default;
			void reset() {
				in_use.clear();
				cv.notify_one();
			}
			connection con;
			std::atomic_flag in_use;
		private:
			std::mutex & cvm;
			std::condition_variable & cv;
		};
		
		struct conview {
			conview() = delete;
			conview(std::shared_ptr<pool_con> ptr) : ptr(ptr) {}
			~conview() {
				if (in_transaction_block) cmd("ROLLBACK");
				ptr->reset();
			}
			
			inline bool ok() { return ptr && ptr->con.ok(); }
			inline result exec(std::string const & cmd) { return ptr->con.exec(cmd); }
			inline result exec_params(std::string const & cmd, std::initializer_list<std::string_view> params) { return ptr->con.exec_params(cmd, std::move(params)); }
			inline bool cmd(std::string const & cmd) { return ptr->con.cmd(cmd); }
			inline bool cmd_params(std::string const & cmd, std::initializer_list<std::string_view> params) { return ptr->con.cmd_params(cmd, std::move(params)); }
			inline void begin() { cmd("BEGIN"); in_transaction_block = true; }
			inline void commit() { cmd("COMMIT"); in_transaction_block = false; }
			inline void rollback() { cmd("ROLLBACK"); in_transaction_block = false; }
		private:
			std::shared_ptr<pool_con> ptr;
			bool in_transaction_block = false;
		};
		
		pool(std::string const & dbname, unsigned int num_cons = std::thread::hardware_concurrency());
		~pool() = default;
		
		inline bool ok() { return ok_; }
		conview try_acquire(); // check ok() before using conview, ALWAYS
		conview acquire(); // blocks until available, no need to check ok()
	private:
		bool ok_ = false;
		std::mutex cvm;
		std::condition_variable cv;
		std::vector<std::shared_ptr<pool_con>> cons;
	};

}
