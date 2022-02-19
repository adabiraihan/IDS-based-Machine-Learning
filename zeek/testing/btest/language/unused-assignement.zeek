# @TEST-EXEC: ZEEK_USAGE_ISSUES=1 zeek -b %INPUT >out 2>&1
# @TEST-EXEC: TEST_DIFF_CANONIFIER=$SCRIPTS/diff-remove-abspath btest-diff out
# @TEST-DOC: The "-u" flags should warn about unused assignments and &is_used suppresses it.

event zeek_init()
	{
	local please_warn: string = "test";
	local please_no_warning: string = "test" &is_used;
	}
