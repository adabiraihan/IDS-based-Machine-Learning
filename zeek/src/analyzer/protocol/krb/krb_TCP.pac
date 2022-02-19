%include binpac.pac
%include zeek.pac

%extern{
namespace zeek::analyzer::krb_tcp { class KRB_Analyzer; }
namespace binpac { namespace KRB_TCP { class KRB_Conn; } }
using KRBTCPAnalyzer = zeek::analyzer::krb_tcp::KRB_Analyzer*;

#include "zeek/zeek-config.h"
#include "zeek/analyzer/protocol/krb/KRB_TCP.h"

#include "zeek/analyzer/protocol/krb/types.bif.h"
#include "zeek/analyzer/protocol/krb/events.bif.h"
%}

extern type KRBTCPAnalyzer;

analyzer KRB_TCP withcontext {
	connection:	KRB_Conn;
	flow:		KRB_Flow;
};

connection KRB_Conn(zeek_analyzer: KRBTCPAnalyzer) {
	upflow = KRB_Flow(true);
	downflow = KRB_Flow(false);
};

%include krb-protocol.pac

flow KRB_Flow(is_orig: bool) {
	flowunit = KRB_PDU_TCP(is_orig) withcontext(connection, this);
};

%include krb-analyzer.pac
