# @TEST-EXEC: zeek -b -C -r $TRACES/tcp/missing-syn.pcap %INPUT
# @TEST-EXEC: btest-diff conn.log

@load base/protocols/http
@load base/frameworks/dpd
@load policy/protocols/conn/mac-logging
