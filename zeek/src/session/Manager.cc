// See the file "COPYING" in the main distribution directory for copyright.

#include "zeek/session/Manager.h"

#include "zeek/zeek-config.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pcap.h>
#include <stdlib.h>
#include <unistd.h>

#include "zeek/Desc.h"
#include "zeek/Event.h"
#include "zeek/NetVar.h"
#include "zeek/Reporter.h"
#include "zeek/RuleMatcher.h"
#include "zeek/RunState.h"
#include "zeek/Timer.h"
#include "zeek/TunnelEncapsulation.h"
#include "zeek/analyzer/Manager.h"
#include "zeek/iosource/IOSource.h"
#include "zeek/packet_analysis/Manager.h"
#include "zeek/session/Session.h"
#include "zeek/telemetry/Manager.h"

zeek::session::Manager* zeek::session_mgr = nullptr;
zeek::session::Manager*& zeek::sessions = zeek::session_mgr;

namespace zeek::session
	{
namespace detail
	{

class ProtocolStats
	{

public:
	struct Protocol
		{
		telemetry::IntGauge active;
		telemetry::IntCounter total;
		ssize_t max = 0;

		Protocol(telemetry::IntGaugeFamily active_family, telemetry::IntCounterFamily total_family,
		         std::string protocol)
			: active(active_family.GetOrAdd({{"protocol", protocol}})),
			  total(total_family.GetOrAdd({{"protocol", protocol}}))
			{
			}
		};

	using ProtocolMap = std::map<std::string, Protocol>;

	ProtocolMap::iterator InitCounters(const std::string& protocol)
		{
		telemetry::IntGaugeFamily active_family = telemetry_mgr->GaugeFamily(
			"zeek", "active-sessions", {"protocol"}, "Active Zeek Sessions");
		telemetry::IntCounterFamily total_family = telemetry_mgr->CounterFamily(
			"zeek", "total-sessions", {"protocol"}, "Total number of sessions", "1", true);

		auto [it, inserted] = entries.insert(
			{protocol, Protocol{active_family, total_family, protocol}});

		if ( inserted )
			return it;

		return entries.end();
		}

	Protocol* GetCounters(const std::string& protocol)
		{
		auto it = entries.find(protocol);
		if ( it == entries.end() )
			it = InitCounters(protocol);

		if ( it != entries.end() )
			return &(it->second);

		return nullptr;
		}

private:
	ProtocolMap entries;
	};

	} // namespace detail

Manager::Manager()
	{
	stats = new detail::ProtocolStats();
	}

Manager::~Manager()
	{
	Clear();
	delete stats;
	}

void Manager::Done() { }

Connection* Manager::FindConnection(Val* v)
	{
	const auto& vt = v->GetType();
	if ( ! IsRecord(vt->Tag()) )
		return nullptr;

	RecordType* vr = vt->AsRecordType();
	auto vl = v->As<RecordVal*>();

	int orig_h, orig_p; // indices into record's value list
	int resp_h, resp_p;

	if ( vr == id::conn_id )
		{
		orig_h = 0;
		orig_p = 1;
		resp_h = 2;
		resp_p = 3;
		}
	else
		{
		// While it's not a conn_id, it may have equivalent fields.
		orig_h = vr->FieldOffset("orig_h");
		resp_h = vr->FieldOffset("resp_h");
		orig_p = vr->FieldOffset("orig_p");
		resp_p = vr->FieldOffset("resp_p");

		if ( orig_h < 0 || resp_h < 0 || orig_p < 0 || resp_p < 0 )
			return nullptr;

		// ### we ought to check that the fields have the right
		// types, too.
		}

	const IPAddr& orig_addr = vl->GetFieldAs<AddrVal>(orig_h);
	const IPAddr& resp_addr = vl->GetFieldAs<AddrVal>(resp_h);

	auto orig_portv = vl->GetFieldAs<PortVal>(orig_p);
	auto resp_portv = vl->GetFieldAs<PortVal>(resp_p);

	zeek::detail::ConnKey conn_key(orig_addr, resp_addr, htons((unsigned short)orig_portv->Port()),
	                               htons((unsigned short)resp_portv->Port()),
	                               orig_portv->PortType(), false);

	return FindConnection(conn_key);
	}

Connection* Manager::FindConnection(const zeek::detail::ConnKey& conn_key)
	{
	detail::Key key(&conn_key, sizeof(conn_key), detail::Key::CONNECTION_KEY_TYPE, false);

	auto it = session_map.find(key);
	if ( it != session_map.end() )
		return static_cast<Connection*>(it->second);

	return nullptr;
	}

void Manager::Remove(Session* s)
	{
	if ( s->IsInSessionTable() )
		{
		s->CancelTimers();
		s->Done();
		s->RemovalEvent();

		detail::Key key = s->SessionKey(false);

		if ( session_map.erase(key) == 0 )
			reporter->InternalWarning("connection missing");
		else
			{
			Connection* c = static_cast<Connection*>(s);
			if ( auto* stat_block = stats->GetCounters(c->TransportIdentifier()) )
				stat_block->active.Dec();
			}

		// Mark that the session isn't in the table so that in case the
		// session has been Ref()'d somewhere, we know that on a future
		// call to Remove() that it's no longer in the map.
		s->SetInSessionTable(false);

		Unref(s);
		}
	}

void Manager::Insert(Session* s, bool remove_existing)
	{
	Session* old = nullptr;
	detail::Key key = s->SessionKey(true);

	if ( remove_existing )
		{
		auto it = session_map.find(key);
		if ( it != session_map.end() )
			old = it->second;

		session_map.erase(key);
		}

	InsertSession(std::move(key), s);

	if ( old && old != s )
		{
		// Some clean-ups similar to those in Remove() (but invisible
		// to the script layer).
		old->CancelTimers();
		old->SetInSessionTable(false);
		Unref(old);
		}
	}

void Manager::Drain()
	{
	// If a random seed was passed in, we're most likely in testing mode and need the
	// order of the sessions to be consistent. Sort the keys to force that order
	// every run.
	if ( zeek::util::detail::have_random_seed() )
		{
		std::vector<const detail::Key*> keys;
		keys.reserve(session_map.size());

		for ( auto& entry : session_map )
			keys.push_back(&(entry.first));
		std::sort(keys.begin(), keys.end(),
		          [](const detail::Key* a, const detail::Key* b)
		          {
					  return *a < *b;
				  });

		for ( const auto* k : keys )
			{
			Session* tc = session_map.at(*k);
			tc->Done();
			tc->RemovalEvent();
			}
		}
	else
		{
		for ( const auto& entry : session_map )
			{
			Session* tc = entry.second;
			tc->Done();
			tc->RemovalEvent();
			}
		}
	}

void Manager::Clear()
	{
	for ( const auto& entry : session_map )
		Unref(entry.second);

	session_map.clear();

	zeek::detail::fragment_mgr->Clear();
	}

void Manager::GetStats(Stats& s)
	{
	auto* tcp_stats = stats->GetCounters("tcp");
	s.max_TCP_conns = tcp_stats->max;
	s.num_TCP_conns = tcp_stats->active.Value();
	s.cumulative_TCP_conns = tcp_stats->total.Value();

	auto* udp_stats = stats->GetCounters("udp");
	s.max_UDP_conns = udp_stats->max;
	s.num_UDP_conns = udp_stats->active.Value();
	s.cumulative_UDP_conns = udp_stats->total.Value();

	auto* icmp_stats = stats->GetCounters("icmp");
	s.max_ICMP_conns = icmp_stats->max;
	s.num_ICMP_conns = icmp_stats->active.Value();
	s.cumulative_ICMP_conns = icmp_stats->total.Value();

	s.num_fragments = zeek::detail::fragment_mgr->Size();
	s.max_fragments = zeek::detail::fragment_mgr->MaxFragments();
	s.num_packets = packet_mgr->PacketsProcessed();
	}

void Manager::Weird(const char* name, const Packet* pkt, const char* addl, const char* source)
	{
	const char* weird_name = name;

	if ( pkt )
		{
		pkt->dump_packet = true;

		if ( pkt->encap && pkt->encap->LastType() != BifEnum::Tunnel::NONE )
			weird_name = util::fmt("%s_in_tunnel", name);

		if ( pkt->ip_hdr )
			{
			reporter->Weird(pkt->ip_hdr->SrcAddr(), pkt->ip_hdr->DstAddr(), weird_name, addl,
			                source);
			return;
			}
		}

	reporter->Weird(weird_name, addl, source);
	}

void Manager::Weird(const char* name, const IP_Hdr* ip, const char* addl)
	{
	reporter->Weird(ip->SrcAddr(), ip->DstAddr(), name, addl);
	}

unsigned int Manager::SessionMemoryUsage()
	{
	unsigned int mem = 0;

	if ( run_state::terminating )
		// Connections have been flushed already.
		return 0;

	for ( const auto& entry : session_map )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		mem += entry.second->MemoryAllocation();
#pragma GCC diagnostic pop

	return mem;
	}

unsigned int Manager::SessionMemoryUsageVals()
	{
	unsigned int mem = 0;

	if ( run_state::terminating )
		// Connections have been flushed already.
		return 0;

	for ( const auto& entry : session_map )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		mem += entry.second->MemoryAllocationVal();
#pragma GCC diagnostic pop

	return mem;
	}

unsigned int Manager::MemoryAllocation()
	{
	if ( run_state::terminating )
		// Connections have been flushed already.
		return 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	return SessionMemoryUsage() + padded_sizeof(*this) +
	       (session_map.size() * (sizeof(SessionMap::key_type) + sizeof(SessionMap::value_type))) +
	       zeek::detail::fragment_mgr->MemoryAllocation();
	// FIXME: MemoryAllocation() not implemented for rest.
	;
#pragma GCC diagnostic pop
	}

void Manager::InsertSession(detail::Key key, Session* session)
	{
	session->SetInSessionTable(true);
	key.CopyData();
	session_map.insert_or_assign(std::move(key), session);

	std::string protocol = session->TransportIdentifier();

	if ( auto* stat_block = stats->GetCounters(protocol) )
		{
		stat_block->active.Inc();
		stat_block->total.Inc();

		if ( stat_block->active.Value() > stat_block->max )
			stat_block->max++;
		}
	}

zeek::detail::PacketFilter* Manager::GetPacketFilter(bool init)
	{
	return packet_mgr->GetPacketFilter(init);
	}

	} // namespace zeek::session
