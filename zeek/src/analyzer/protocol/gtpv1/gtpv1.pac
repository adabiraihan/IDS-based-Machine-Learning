%include binpac.pac
%include zeek.pac

%extern{
#include "zeek/IP.h"
#include "zeek/TunnelEncapsulation.h"
#include "zeek/Reporter.h"

#include "zeek/analyzer/protocol/gtpv1/events.bif.h"
%}

analyzer GTPv1 withcontext {
	connection:	GTPv1_Conn;
	flow:		GTPv1_Flow;
};

%include gtpv1-protocol.pac
%include gtpv1-analyzer.pac
