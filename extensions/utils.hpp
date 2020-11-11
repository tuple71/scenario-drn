#ifndef NDNSIM_UTILS_HPP
#define NDNSIM_UTILS_HPP

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <algorithm>

#include <ndn-cxx/encoding/buffer.hpp>
#include <ns3/ndnSIM/model/ndn-common.hpp>

typedef std::vector<std::string>::iterator StringListIterator;
typedef std::map<std::string, std::string>::iterator StringMapIterator;

typedef std::shared_ptr<std::vector<std::string>> StringListPtr;
typedef std::map<std::string, StringListPtr>::iterator StringListMapIterator;

typedef std::shared_ptr<::ndn::Buffer> BufferPtr;
typedef std::vector<::ndn::BufferPtr>::iterator BufferListIterator;

typedef std::shared_ptr<std::vector<BufferPtr>> BufferListPtr;
typedef std::map<std::string, BufferListPtr>::iterator BufferListMapIterator;

typedef long long int lli;

std::string
stringf(const std::string fmt, ...);

// trim from start
static inline
std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

// trim from end
static inline
std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

// trim from both ends
static inline
std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}

static inline
std::pair<std::string, std::string> parsePair(std::string src, char c) {
	std::pair<std::string, std::string> v;

	std::string::size_type s1 = 0, s2;
	if((s2 = src.find(c, s1)) != std::string::npos) {
		std::string key = src.substr(s1, s2-s1);
		s1 = s2 + 1;
		v.first = trim(key);
		v.second = src.substr(s1);
	} else {
		v.first = trim(src);
		v.second = "";
	}

	return v;
}

static inline
std::vector<std::string> split(std::string &src, char c) {
	std::vector<std::string> v;

	std::string::size_type s1 = 0, s2;
	while((s2 = src.find(c, s1)) != std::string::npos) {
		std::string topic = src.substr(s1, s2-s1);
		s1 = s2 + 1;
		v.push_back(trim(topic));
	}
	std::string topic = src.substr(s1);
	v.push_back(trim(topic));

	return v;
}

constexpr char toHexChar(unsigned int n, bool wantUpperCase = true) noexcept {
  return wantUpperCase ?
         "0123456789ABCDEF"[n & 0xf] :
         "0123456789abcdef"[n & 0xf];
}

constexpr int fromHexChar(char c) noexcept {
  return (c >= '0' && c <= '9') ? int(c - '0') :
         (c >= 'A' && c <= 'F') ? int(c - 'A' + 10) :
         (c >= 'a' && c <= 'f') ? int(c - 'a' + 10) :
         -1;
}

std::string escape(const std::string& str);

void escape(std::ostream& os, const char* str, size_t len);

std::string unescape(const std::string& str);

void unescape(std::ostream& os, const char* str, size_t len);

lli getHash(std::string key);

#endif // NDNSIM_UTILS_HPP
