// See the file  in the main distribution directory for copyright.

#include "zeek/plugin/Plugin.h"

#include "zeek/input/readers/binary/Binary.h"

namespace zeek::plugin::detail::Zeek_BinaryReader
	{

class Plugin : public zeek::plugin::Plugin
	{
public:
	zeek::plugin::Configuration Configure() override
		{
		AddComponent(
			new zeek::input::Component("Binary", zeek::input::reader::detail::Binary::Instantiate));

		zeek::plugin::Configuration config;
		config.name = "Zeek::BinaryReader";
		config.description = "Binary input reader";
		return config;
		}
	} plugin;

	} // namespace zeek::plugin::detail::Zeek_BinaryReader
