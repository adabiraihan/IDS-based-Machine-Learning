#include "broker/data.hh"

#include <caf/hash/fnv.hpp>
#include <caf/node_id.hpp>

#include "broker/convert.hh"

namespace broker {

struct type_name_getter {
  using result_type = const char*;

  result_type operator()(broker::address) {
    return "address";
  }

  result_type operator()(broker::boolean) {
    return "boolean";
  }

  result_type operator()(broker::count) {
    return "count";
  }

  result_type operator()(broker::enum_value) {
    return "enum value";
  }

  result_type operator()(broker::integer) {
    return "integer";
  }

  result_type operator()(broker::none) {
    return "none";
  }

  result_type operator()(broker::port) {
    return "port";
  }

  result_type operator()(broker::real) {
    return "real";
  }

  result_type operator()(broker::set) {
    return "set";
  }

  result_type operator()(std::string) {
    return "string";
  }

  result_type operator()(broker::subnet) {
    return "subnet";
  }

  result_type operator()(broker::table) {
    return "table";
  }

  result_type operator()(broker::timespan) {
    return "timespan";
  }

  result_type operator()(broker::timestamp) {
    return "timestamp";
  }

  result_type operator()(broker::vector) {
    return "vector";
  }
};

struct type_getter {
  using result_type = data::type;

  result_type operator()(broker::address) {
    return data::type::address;
  }

  result_type operator()(broker::boolean) {
    return data::type::boolean;
  }

  result_type operator()(broker::count) {
    return data::type::count;
  }

  result_type operator()(broker::enum_value) {
    return data::type::enum_value;
  }

  result_type operator()(broker::integer) {
    return data::type::integer;
  }

  result_type operator()(broker::none) {
    return data::type::none;
  }

  result_type operator()(broker::port) {
    return data::type::port;
  }

  result_type operator()(broker::real) {
    return data::type::real;
  }

  result_type operator()(broker::set) {
    return data::type::set;
  }

  result_type operator()(std::string) {
    return data::type::string;
  }

  result_type operator()(broker::subnet) {
    return data::type::subnet;
  }

  result_type operator()(broker::table) {
    return data::type::table;
  }

  result_type operator()(broker::timespan) {
    return data::type::timespan;
  }

  result_type operator()(broker::timestamp) {
    return data::type::timestamp;
  }

  result_type operator()(broker::vector) {
    return data::type::vector;
  }
};

data::type data::get_type() const {
  return caf::visit(type_getter(), get_data());
}

data data::from_type(data::type t) {
  switch ( t ) {
  case data::type::address:
    return broker::address{};
  case data::type::boolean:
    return broker::boolean{};
  case data::type::count:
    return broker::count{};
  case data::type::enum_value:
    return broker::enum_value{};
  case data::type::integer:
    return broker::integer{};
  case data::type::none:
    return broker::data{};
  case data::type::port:
    return broker::port{};
  case data::type::real:
    return broker::real{};
  case data::type::set:
    return broker::set{};
  case data::type::string:
    return std::string{};
  case data::type::subnet:
    return broker::subnet{};
  case data::type::table:
    return broker::table{};
  case data::type::timespan:
    return broker::timespan{};
  case data::type::timestamp:
    return broker::timestamp{};
  case data::type::vector:
    return broker::vector{};
  default:
    return data{};
  }
}

const char* data::get_type_name() const {
  return caf::visit(type_name_getter(), *this);
}

namespace {

template <class Container>
void container_convert(Container& c, std::string& str,
                       const char* left, const char* right,
                       const char* delim = ", ") {
  auto first = begin(c);
  auto last = end(c);
  str += left;
  if (first != last) {
    str += to_string(*first);
    while (++first != last)
      str += delim + to_string(*first);
  }
  str += right;
}

struct data_converter {
  using result_type = bool;

  template <class T>
  result_type operator()(const T& x) {
    return convert(x, str);
  }

  result_type operator()(bool b) {
    str = b ? 'T' : 'F';
    return true;
  }

  result_type operator()(const std::string& x) {
    str = x;
    return true;
  }

  std::string& str;
};

} // namespace <anonymous>

bool convert(const table::value_type& e, std::string& str) {
  str += to_string(e.first) + " -> " + to_string(e.second);
  return true;
}

bool convert(const vector& v, std::string& str) {
  container_convert(v, str, "(", ")");
  return true;
}

bool convert(const set& s, std::string& str) {
  container_convert(s, str, "{", "}");
  return true;
}

bool convert(const table& t, std::string& str) {
  container_convert(t, str, "{", "}");
  return true;
}

bool convert(const data& d, std::string& str) {
  caf::visit(data_converter{str}, d);
  return true;
}

bool convert(const data& d, caf::node_id& node){
  if (is<std::string>(d))
    if (auto err = caf::parse(get<std::string>(d), node); !err)
      return true;
  return false;
}

bool convert(const caf::node_id& node, data& d) {
  if (node)
    d = to_string(node);
  else
    d = nil;
  return true;
}

} // namespace broker

namespace broker::detail {

size_t fnv_hash(const broker::data& x) {
  return caf::hash::fnv<size_t>::compute(x);
}

size_t fnv_hash(const broker::set& x) {
  return caf::hash::fnv<size_t>::compute(x);
}

size_t fnv_hash(const broker::vector& x) {
  return caf::hash::fnv<size_t>::compute(x);
}

size_t fnv_hash(const broker::table::value_type& x) {
  return caf::hash::fnv<size_t>::compute(x);
}

size_t fnv_hash(const broker::table& x) {
  return caf::hash::fnv<size_t>::compute(x);
}

} // namespace broker::detail
