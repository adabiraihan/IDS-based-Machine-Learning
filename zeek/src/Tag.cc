// See the file "COPYING" in the main distribution directory for copyright.

#include "zeek/Tag.h"

#include "zeek/Val.h"

namespace zeek
	{

Tag::Tag(const EnumTypePtr& etype, type_t arg_type, subtype_t arg_subtype)
	{
	assert(arg_type > 0);

	type = arg_type;
	subtype = arg_subtype;
	int64_t i = (int64_t)(type) | ((int64_t)subtype << 31);
	val = etype->GetEnumVal(i);
	}

Tag::Tag(EnumValPtr arg_val)
	{
	assert(arg_val);

	val = std::move(arg_val);

	int64_t i = val->InternalInt();
	type = i & 0xffffffff;
	subtype = (i >> 31) & 0xffffffff;
	}

Tag::Tag(const Tag& other)
	{
	type = other.type;
	subtype = other.subtype;
	val = other.val;
	}

Tag::Tag()
	{
	type = 0;
	subtype = 0;
	val = nullptr;
	}

Tag::~Tag() = default;

Tag& Tag::operator=(const Tag& other)
	{
	if ( this != &other )
		{
		type = other.type;
		subtype = other.subtype;
		val = other.val;
		}

	return *this;
	}

Tag& Tag::operator=(const Tag&& other) noexcept
	{
	if ( this != &other )
		{
		type = other.type;
		subtype = other.subtype;
		val = std::move(other.val);
		}

	return *this;
	}

const EnumValPtr& Tag::AsVal(const EnumTypePtr& etype) const
	{
	if ( ! val )
		{
		assert(type == 0 && subtype == 0);
		val = etype->GetEnumVal(0);
		}

	return val;
	}

std::string Tag::AsString() const
	{
	return util::fmt("%" PRIu32 "/%" PRIu32, type, subtype);
	}

	} // namespace zeek
