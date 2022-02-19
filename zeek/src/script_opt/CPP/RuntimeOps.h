// See the file "COPYING" in the main distribution directory for copyright.

// Run-time support for (non-vector) operations in C++-compiled scripts.

#pragma once

#include "zeek/Val.h"
#include "zeek/script_opt/CPP/Func.h"

namespace zeek
	{

using SubNetValPtr = IntrusivePtr<zeek::SubNetVal>;

namespace detail
	{

// Returns the concatenation of the given strings.
extern StringValPtr str_concat__CPP(const String* s1, const String* s2);

// Returns true if string "s2" is in string "s1".
extern bool str_in__CPP(const String* s1, const String* s2);

// Converts a vector of individual ValPtr's into a single ListValPtr
// suitable for indexing an aggregate.
extern ListValPtr index_val__CPP(std::vector<ValPtr> indices);

// Returns the value corresponding to indexing the given table/vector/string
// with the given set of indices.  These are functions rather than something
// generated directly so that they can package up the error handling for
// the case where there's no such index.
extern ValPtr index_table__CPP(const TableValPtr& t, std::vector<ValPtr> indices);
extern ValPtr index_vec__CPP(const VectorValPtr& vec, int index);
extern ValPtr index_string__CPP(const StringValPtr& svp, std::vector<ValPtr> indices);

// Calls out to the given script or BiF function.  A separate function because
// of the need to (1) construct the "args" vector using {} initializers,
// but (2) needing to have the address of that vector.
inline ValPtr invoke__CPP(Func* f, std::vector<ValPtr> args, Frame* frame)
	{
	return f->Invoke(&args, frame);
	}

// Assigns the given value to the given global.  A separate function because
// we also need to return the value, for use in assignment cascades.
inline ValPtr set_global__CPP(IDPtr g, ValPtr v)
	{
	g->SetVal(v);
	return v;
	}

// Assigns the given global to the given value, which corresponds to an
// event handler.
extern ValPtr set_event__CPP(IDPtr g, ValPtr v, EventHandlerPtr& gh);

// Convert (in terms of the Zeek language) the given value to the given type.
// A separate function in order to package up the error handling.
extern ValPtr cast_value_to_type__CPP(const ValPtr& v, const TypePtr& t);

// Convert a value of type "any" to the given concrete type.  A separate
// function in order to package up the error handling.
extern ValPtr from_any__CPP(const ValPtr& v, const TypePtr& t);

// Convert a vector-of-any to a vector-of-t.  A separate function in order
// to package up the error handling.
extern ValPtr from_any_vec__CPP(const ValPtr& v, const TypePtr& t);

// Returns the subnet corresponding to the given mask of the given address.
// A separate function in order to package up the error handling.
extern SubNetValPtr addr_mask__CPP(const IPAddr& a, uint32_t mask);

// Assigns the given field in the given record to the given value.  A
// separate function to allow for assignment cascades.
inline ValPtr assign_field__CPP(RecordValPtr rec, int field, ValPtr v)
	{
	rec->Assign(field, v);
	return v;
	}

// Returns the given field in the given record.  A separate function to
// support error handling.
inline ValPtr field_access__CPP(const RecordValPtr& rec, int field)
	{
	auto v = rec->GetFieldOrDefault(field);
	if ( ! v )
		reporter->CPPRuntimeError("field value missing");

	return v;
	}

// Each of the following executes the assignment "v1[v2] = v3" for
// tables/vectors/strings.
extern ValPtr assign_to_index__CPP(TableValPtr v1, ValPtr v2, ValPtr v3);
extern ValPtr assign_to_index__CPP(VectorValPtr v1, ValPtr v2, ValPtr v3);
extern ValPtr assign_to_index__CPP(StringValPtr v1, ValPtr v2, ValPtr v3);

// Executes an "add" statement for the given set.
extern void add_element__CPP(TableValPtr aggr, ListValPtr indices);

// Executes a "delete" statement for the given set.
extern void remove_element__CPP(TableValPtr aggr, ListValPtr indices);

// Returns the given table/set (which should be empty) coerced to
// the given Zeek type.  A separate function in order to deal with
// error handling.  Inlined because this gets invoked a lot.
inline TableValPtr table_coerce__CPP(const ValPtr& v, const TypePtr& t)
	{
	TableVal* tv = v->AsTableVal();

	if ( tv->Size() > 0 )
		reporter->CPPRuntimeError("coercion of non-empty table/set");

	return make_intrusive<TableVal>(cast_intrusive<TableType>(t), tv->GetAttrs());
	}

// The same, for an empty record.
inline VectorValPtr vector_coerce__CPP(const ValPtr& v, const TypePtr& t)
	{
	VectorVal* vv = v->AsVectorVal();

	if ( vv->Size() > 0 )
		reporter->CPPRuntimeError("coercion of non-empty vector");

	return make_intrusive<VectorVal>(cast_intrusive<VectorType>(t));
	}

// Constructs a set of the given type, containing the given elements, and
// with the associated attributes.
extern TableValPtr set_constructor__CPP(std::vector<ValPtr> elements, TableTypePtr t,
                                        std::vector<int> attr_tags, std::vector<ValPtr> attr_vals);

// Constructs a table of the given type, containing the given elements
// (specified as parallel index/value vectors), and with the associated
// attributes.
extern TableValPtr table_constructor__CPP(std::vector<ValPtr> indices, std::vector<ValPtr> vals,
                                          TableTypePtr t, std::vector<int> attr_tags,
                                          std::vector<ValPtr> attr_vals);

// Constructs a record of the given type, whose (ordered) fields are
// assigned to the corresponding elements of the given vector of values.
extern RecordValPtr record_constructor__CPP(std::vector<ValPtr> vals, RecordTypePtr t);

// Constructs a vector of the given type, populated with the given values.
extern VectorValPtr vector_constructor__CPP(std::vector<ValPtr> vals, VectorTypePtr t);

// Schedules an event to occur at the given absolute time, parameterized
// with the given set of values.  A separate function to facilitate avoiding
// the scheduling if Zeek is terminating.
extern ValPtr schedule__CPP(double dt, EventHandlerPtr event, std::vector<ValPtr> args);

// Simple helper functions for supporting absolute value.
inline bro_uint_t iabs__CPP(bro_int_t v)
	{
	return v < 0 ? -v : v;
	}

inline double fabs__CPP(double v)
	{
	return v < 0.0 ? -v : v;
	}

// The following operations are provided using functions to support
// error checking/reporting.
inline bro_int_t idiv__CPP(bro_int_t v1, bro_int_t v2)
	{
	if ( v2 == 0 )
		reporter->CPPRuntimeError("division by zero");
	return v1 / v2;
	}

inline bro_int_t imod__CPP(bro_int_t v1, bro_int_t v2)
	{
	if ( v2 == 0 )
		reporter->CPPRuntimeError("modulo by zero");
	return v1 % v2;
	}

inline bro_uint_t udiv__CPP(bro_uint_t v1, bro_uint_t v2)
	{
	if ( v2 == 0 )
		reporter->CPPRuntimeError("division by zero");
	return v1 / v2;
	}

inline bro_uint_t umod__CPP(bro_uint_t v1, bro_uint_t v2)
	{
	if ( v2 == 0 )
		reporter->CPPRuntimeError("modulo by zero");
	return v1 % v2;
	}

inline double fdiv__CPP(double v1, double v2)
	{
	if ( v2 == 0.0 )
		reporter->CPPRuntimeError("division by zero");
	return v1 / v2;
	}

	} // namespace zeek::detail
	} // namespace zeek
