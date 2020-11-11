
#include "utils.hpp"

#include <cmath>
#include <string>
#include <stdarg.h>

#include "Chord-DHT/sha1.hpp"
#include "Chord-DHT/M.h"


std::string 
stringf(const std::string fmt, ...) {
	// use a rubric appropriate for your code
	int n, size;
	std::string str;
	va_list ap;

	va_start(ap, fmt);
	n = vsnprintf((char *)'\0', 0, fmt.c_str(), ap);
	va_end(ap);

	size = n + 1;
	// maximum 2 passes on a POSIX system...
	while (1) {
		str.resize(size);
		va_start(ap, fmt);
		int n = vsnprintf((char *) str.data(), size, fmt.c_str(), ap);
		va_end(ap);
		// everything worked
		if (n > -1 && n < size) {
			str.resize(n);
			return str;
		}
		// needed size returned
		if (n > -1) {
			// for null char
			size = n + 1;
		} else {
			// guess at a larger size (o/s specific)
			size *= 2;
		}
	}
	return str;
}


#if 0
Block::Block(const uint8_t* buf, size_t bufSize)
{
  const uint8_t* pos = buf;
  const uint8_t* const end = buf + bufSize;

  m_type = tlv::readType(pos, end);
  uint64_t length = tlv::readVarNumber(pos, end);
  // pos now points to TLV-VALUE

  BOOST_ASSERT(pos <= end);
  if (length > static_cast<uint64_t>(end - pos)) {
    BOOST_THROW_EXCEPTION(Error("Not enough bytes in the buffer to fully parse TLV"));
  }

  BOOST_ASSERT(pos > buf);
  uint64_t typeLengthSize = static_cast<uint64_t>(pos - buf);
  m_size = typeLengthSize + length;

  m_buffer = make_shared<Buffer>(buf, m_size);
  m_begin = m_buffer->begin();
  m_end = m_valueEnd = m_buffer->end();
  m_valueBegin = m_begin + typeLengthSize;
}
#endif


std::string 
escape(const std::string& str)
{
  std::ostringstream os;
  escape(os, str.data(), str.size());
  return os.str();
}

void 
escape(std::ostream& os, const char* str, size_t len)
{
  for (size_t i = 0; i < len; ++i) {
    auto c = str[i];
    // Unreserved characters don't need to be escaped.
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '.' ||
        c == '_' || c == '~') {
      os << c;
    }
    else {
      os << '%';
      os << toHexChar((c & 0xf0) >> 4);
      os << toHexChar(c & 0xf);
    }
  }
}

std::string 
unescape(const std::string& str)
{
  std::ostringstream os;
  unescape(os, str.data(), str.size());
  return os.str();
}

void 
unescape(std::ostream& os, const char* str, size_t len)
{
  for (size_t i = 0; i < len; ++i) {
    if (str[i] == '%' && i + 2 < len) {
      int hi = fromHexChar(str[i + 1]);
      int lo = fromHexChar(str[i + 2]);

      if (hi < 0 || lo < 0)
        // Invalid hex characters, so just keep the escaped string.
        os << str[i] << str[i + 1] << str[i + 2];
      else
        os << static_cast<char>((hi << 4) | lo);

      // Skip ahead past the escaped value.
      i += 2;
    }
    else {
      // Just copy through.
      os << str[i];
    }
  }
}

/* get SHA1 hash for a given key */
lli 
getHash(std::string key){
	char hex[SHA1_HEX_SIZE];
	char finalHash[41];
	std::string keyHash = "";
	size_t i;
	sha1 s;

	s.add(key.c_str(), key.size());
	s.finalize();
	s.print_hex(hex);

	lli mod = pow(2, M);

	for (i = 0; i < M / 8; i++) {
		sprintf(finalHash, "%02x", hex[i]);
		keyHash += finalHash;
	}
	lli hash = strtoll(keyHash.c_str(), NULL, 16) % mod;

	return hash;
}
