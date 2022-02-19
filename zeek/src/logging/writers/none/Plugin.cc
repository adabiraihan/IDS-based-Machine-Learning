// See the file  in the main distribution directory for copyright.

#include "zeek/plugin/Plugin.h"

#include "zeek/logging/writers/none/None.h"

namespace zeek::plugin::detail::Zeek_NoneWriter
	{

class Plugin : public zeek::plugin::Plugin
	{
public:
	zeek::plugin::Configuration Configure() override
		{
		AddComponent(
			new zeek::logging::Component("None", zeek::logging::writer::detail::None::Instantiate));

		zeek::plugin::Configuration config;
		config.name = "Zeek::NoneWriter";
		config.description = "None log writer (primarily for debugging)";
		return config;
		}
	} plugin;

	} // namespace zeek::plugin::detail::Zeek_NoneWriter
