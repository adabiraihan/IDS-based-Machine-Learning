#
# @TEST-EXEC: zeek -b %INPUT >out
# @TEST-EXEC: btest-diff out

global a = "zeek test";

type Color: enum { RED, GREEN, BLUE };

event zeek_init()
	{
	local b = "local value";

	print lookup_ID("a");
	print lookup_ID("");
	print lookup_ID("xyz");
	print lookup_ID("b");
	print lookup_ID("GREEN");
	print type_name( lookup_ID("zeek_init") );
	}
