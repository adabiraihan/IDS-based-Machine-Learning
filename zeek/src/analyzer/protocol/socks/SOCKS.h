#pragma once

// SOCKS v4 analyzer.

#include "zeek/analyzer/protocol/pia/PIA.h"
#include "zeek/analyzer/protocol/tcp/TCP.h"

namespace binpac
	{
namespace SOCKS
	{
class SOCKS_Conn;
	}
	}

namespace zeek::analyzer::socks
	{

class SOCKS_Analyzer final : public analyzer::tcp::TCP_ApplicationAnalyzer
	{
public:
	explicit SOCKS_Analyzer(Connection* conn);
	~SOCKS_Analyzer() override;

	void EndpointDone(bool orig);

	void Done() override;
	void DeliverStream(int len, const u_char* data, bool orig) override;
	void Undelivered(uint64_t seq, int len, bool orig) override;
	void EndpointEOF(bool is_orig) override;

	static analyzer::Analyzer* Instantiate(Connection* conn) { return new SOCKS_Analyzer(conn); }

protected:
	bool orig_done;
	bool resp_done;

	analyzer::pia::PIA_TCP* pia;
	binpac::SOCKS::SOCKS_Conn* interp;
	};

	} // namespace zeek::analyzer::socks
