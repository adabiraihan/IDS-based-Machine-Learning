	function proc_client_hello(
					version : uint16, ts : double,
					client_random : bytestring,
					session_id : uint8[],
					cipher_suites16 : uint16[],
					cipher_suites24 : uint24[],
					compression_methods: uint8[]) : bool
		%{
		if ( ! version_ok(version) )
			{
			zeek_analyzer()->ProtocolViolation(zeek::util::fmt("unsupported client SSL version 0x%04x", version));
			zeek_analyzer()->SetSkip(true);
			}
		else
			zeek_analyzer()->ProtocolConfirmation();

		if ( ssl_client_hello )
			{
			vector<int> cipher_suites;

			if ( cipher_suites16 )
				std::copy(cipher_suites16->begin(), cipher_suites16->end(), std::back_inserter(cipher_suites));
			else
				std::transform(cipher_suites24->begin(), cipher_suites24->end(), std::back_inserter(cipher_suites), to_int());

			auto cipher_vec = zeek::make_intrusive<zeek::VectorVal>(zeek::id::index_vec);

			for ( unsigned int i = 0; i < cipher_suites.size(); ++i )
				{
				auto ciph = zeek::val_mgr->Count(cipher_suites[i]);
				cipher_vec->Assign(i, ciph);
				}

			auto comp_vec = zeek::make_intrusive<zeek::VectorVal>(zeek::id::index_vec);

			if ( compression_methods )
				{
				for ( unsigned int i = 0; i < compression_methods->size(); ++i )
					{
					auto comp = zeek::val_mgr->Count((*compression_methods)[i]);
					comp_vec->Assign(i, comp);
					}
				}

			zeek::BifEvent::enqueue_ssl_client_hello(zeek_analyzer(), zeek_analyzer()->Conn(),
							version, record_version(), ts,
							zeek::make_intrusive<zeek::StringVal>(client_random.length(),
							                                      (const char*) client_random.data()),
							{zeek::AdoptRef{}, to_string_val(session_id)},
							std::move(cipher_vec), std::move(comp_vec));
			}

		return true;
		%}
