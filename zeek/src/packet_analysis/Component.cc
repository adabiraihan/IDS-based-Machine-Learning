// See the file "COPYING" in the main distribution directory for copyright.

#include "zeek/packet_analysis/Component.h"

#include "zeek/Desc.h"
#include "zeek/packet_analysis/Manager.h"

using namespace zeek::packet_analysis;

Component::Component(const std::string& name, factory_callback arg_factory,
                     Tag::subtype_t arg_subtype)
	: plugin::Component(plugin::component::PACKET_ANALYZER, name),
	  plugin::TaggedComponent<packet_analysis::Tag>(arg_subtype)
	{
	factory = arg_factory;
	}

void Component::Initialize()
	{
	InitializeTag();
	packet_mgr->RegisterComponent(this, "ANALYZER_");
	}

void Component::DoDescribe(ODesc* d) const
	{
	if ( factory )
		{
		d->Add("ANALYZER_");
		d->Add(CanonicalName());
		}
	}
