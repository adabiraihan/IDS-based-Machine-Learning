// See the file  in the main distribution directory for copyright.

#include "zeek/plugin/Plugin.h"

#include "zeek/file_analysis/Component.h"
#include "zeek/file_analysis/analyzer/pe/PE.h"

namespace zeek::plugin::detail::Zeek_PE
	{

class Plugin : public zeek::plugin::Plugin
	{
public:
	zeek::plugin::Configuration Configure() override
		{
		AddComponent(
			new zeek::file_analysis::Component("PE", zeek::file_analysis::detail::PE::Instantiate));

		zeek::plugin::Configuration config;
		config.name = "Zeek::PE";
		config.description = "Portable Executable analyzer";
		return config;
		}
	} plugin;

	} // namespace zeek::plugin::detail::Zeek_PE
