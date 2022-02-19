// See the file "COPYING" in the main distribution directory for copyright.

#pragma once

#include <memory>

#include "zeek/IntrusivePtr.h"
#include "zeek/Type.h"

namespace zeek
	{

class ListVal;
using ListValPtr = zeek::IntrusivePtr<ListVal>;

	} // namespace zeek

namespace zeek::detail
	{

class HashKey;

class CompositeHash
	{
public:
	explicit CompositeHash(TypeListPtr composite_type);

	// Compute the hash corresponding to the given index val,
	// or nullptr if it fails to typecheck.
	std::unique_ptr<HashKey> MakeHashKey(const Val& v, bool type_check) const;

	// Given a hash key, recover the values used to create it.
	ListValPtr RecoverVals(const HashKey& k) const;

	[[deprecated("Remove in v5.1. MemoryAllocation() is deprecated and will be removed. See "
	             "GHI-572.")]] unsigned int
	MemoryAllocation() const
		{
		return padded_sizeof(*this);
		}

protected:
	bool SingleValHash(HashKey& hk, const Val* v, Type* bt, bool type_check, bool optional,
	                   bool singleton) const;

	// Recovers just one Val of possibly many; called from RecoverVals.
	// Upon return, pval will point to the recovered Val of type t.
	// Returns and updated kp for the next Val.  Calls reporter->InternalError()
	// upon errors, so there is no return value for invalid input.
	bool RecoverOneVal(const HashKey& k, Type* t, ValPtr* pval, bool optional,
	                   bool singleton) const;

	// Compute the size of the composite key.  If v is non-nil then
	// the value is computed for the particular list of values.
	// Returns 0 if the key has an indeterminant size (if v not given),
	// or if v doesn't match the index type (if given).
	bool ReserveKeySize(HashKey& hk, const Val* v, bool type_check, bool calc_static_size) const;

	bool ReserveSingleTypeKeySize(HashKey& hk, Type*, const Val* v, bool type_check, bool optional,
	                              bool calc_static_size, bool singleton) const;

	bool EnsureTypeReserve(HashKey& hk, const Val* v, Type* bt, bool type_check) const;

	TypeListPtr type;
	bool is_singleton = false; // if just one type in index
	};

	} // namespace zeek::detail
