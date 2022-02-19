// See the file  in the main distribution directory for copyright.

#include "zeek/plugin/Plugin.h"

#include "zeek/analyzer/Component.h"
#include "zeek/analyzer/protocol/netbios/NetbiosSSN.h"

namespace zeek::plugin::detail::Zeek_NetBIOS
	{

class Plugin : public zeek::plugin::Plugin
	{
public:
	zeek::plugin::Configuration Configure() override
		{
		AddComponent(new zeek::analyzer::Component(
			"NetbiosSSN", zeek::analyzer::netbios_ssn::NetbiosSSN_Analyzer::Instantiate));
		AddComponent(new zeek::analyzer::Component("Contents_NetbiosSSN", nullptr));

		zeek::plugin::Configuration config;
		config.name = "Zeek::NetBIOS";
		config.description = "NetBIOS analyzer support";
		return config;
		}
	} plugin;

	} // namespace zeek::plugin::detail::Zeek_NetBIOS
