/*
 * Copyright 2022 Axel Waggershauser
 */
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <cstdint>

namespace ZXing {

/**
 * @brief The Error class is a value type for the error() member of @Barcode
 *
 * The use-case of this class is to communicate whether or not a particular Barcode
 * symbol is in error. It is (primarily) not meant to be thrown as an exception and
 * therefore not derived from std::exception. The library code may throw (and catch!)
 * objects of this class as a convenient means of flow control (c++23's std::expected
 * will allow to replace those use-cases with something similarly convenient). In
 * those situations, the author is advised to make sure any thrown Error object is
 * caught before leaking into user/wrapper code, i.e. the functions of the public
 * API should be considered `noexcept` with respect to this class.
 *
 * Looking at the implementation of std::runtime_exception, it might actually be of
 * interest to replace the std::string msg member with a std::runtime_exception base
 * class, thereby reducing sizeof(Error) by 16 bytes. This would be a breaking ABI
 * change and would therefore have to wait for release 3.0. (TODO)
 */

class Error
{
public:
	enum class Type : uint8_t { None, Format, Checksum, Unsupported };
	Type type() const noexcept { return _type; }
	const std::string& msg() const noexcept { return _msg; }
	explicit operator bool() const noexcept { return _type != Type::None; }

	std::string location() const;

	Error() = default;
	Error(Type type, std::string msg = {}) : _msg(std::move(msg)), _type(type) {}
	Error(const char* file, short line, Type type, std::string msg = {}) : _msg(std::move(msg)), _file(file), _line(line), _type(type) {}

	static constexpr auto Format = Type::Format;
	static constexpr auto Checksum = Type::Checksum;
	static constexpr auto Unsupported = Type::Unsupported;

	inline bool operator==(const Error& o) const noexcept
	{
		return _type == o._type && _msg == o._msg && _file == o._file && _line == o._line;
	}
	inline bool operator!=(const Error& o) const noexcept { return !(*this == o); }

protected:
	std::string _msg;
	const char* _file = nullptr;
	short _line = -1;
	Type _type = Type::None;
};

inline bool operator==(const Error& e, Error::Type t) noexcept { return e.type() == t; }
inline bool operator!=(const Error& e, Error::Type t) noexcept { return !(e == t); }
inline bool operator==(Error::Type t, const Error& e) noexcept { return e.type() == t; }
inline bool operator!=(Error::Type t, const Error& e) noexcept { return !(t == e); }

#define FormatError(...) Error(__FILE__, __LINE__, Error::Format, std::string(__VA_ARGS__))
#define ChecksumError(...) Error(__FILE__, __LINE__, Error::Checksum, std::string(__VA_ARGS__))
#define UnsupportedError(...) Error(__FILE__, __LINE__, Error::Unsupported, std::string(__VA_ARGS__))

std::string ToString(const Error& e);

}
