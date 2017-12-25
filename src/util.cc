#include "util.hh"

#include <botan/auto_rng.h>
#include <botan/lookup.h>
#include <botan/cipher_mode.h>

#include <mutex>
#include <iostream>

static Botan::AutoSeeded_RNG rng {};
static std::unique_ptr<Botan::HashFunction> blake2b = nullptr;

static size_t randrange(size_t min, size_t max) { // [min,max)
	size_t i, r = max - min;
	rng.randomize(reinterpret_cast<uint8_t *>(&i), sizeof(i));
	return min + (i % r);
}

void rainboa::util::init() {
	blake2b = Botan::HashFunction::create("Blake2b");
	if (!blake2b) {
		throwe(startup);
	}
}

void rainboa::util::term() noexcept {
	blake2b.reset();
}

static std::mutex log_mut {};
void rainboa::util::log(log_level, std::string const & str) {
	log_mut.lock();
	std::cout << str << std::endl;
	log_mut.unlock();
}

asterid::buffer_assembly rainboa::util::random(size_t len) {
	auto vec = rng.random_vec(len);
	asterid::buffer_assembly bb {};
	bb.resize(len);
	memcpy(bb.data(), vec.data(), len);
	return bb;
}

std::string rainboa::util::random_str(size_t len, std::string const & chars) {
	std::string ret;
	for (size_t i = 0; i < len; i++) {
		ret += chars[randrange(0, chars.size())];
	}
	return ret;
}

asterid::buffer_assembly rainboa::util::hash_blake2b(std::string str) {
	auto buf = blake2b->process(str);
	asterid::buffer_assembly bb {};
	bb.resize(buf.size());
	memcpy(bb.data(), buf.data(), bb.size());
	return bb;
}

void rainboa::util::randomize_data(void * ptr, size_t len) {
	rng.randomize(reinterpret_cast<uint8_t *>(ptr), sizeof(len));
}
