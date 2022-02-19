##! Base SSL analysis script.  This script logs information about the SSL/TLS
##! handshaking and encryption establishment process.

@load base/frameworks/notice/weird
@load ./consts
@load base/protocols/conn/removal-hooks

module SSL;

export {
	redef enum Log::ID += { LOG };

	global log_policy: Log::PolicyHook;

	## The record type which contains the fields of the SSL log.
	type Info: record {
		## Time when the SSL connection was first detected.
		ts:               time             &log;
		## Unique ID for the connection.
		uid:              string           &log;
		## The connection's 4-tuple of endpoint addresses/ports.
		id:               conn_id          &log;
		## Numeric SSL/TLS version that the server chose.
		version_num:      count            &optional;
		## SSL/TLS version that the server chose.
		version:          string           &log &optional;
		## SSL/TLS cipher suite that the server chose.
		cipher:           string           &log &optional;
		## Elliptic curve the server chose when using ECDH/ECDHE.
		curve:            string           &log &optional;
		## Value of the Server Name Indicator SSL/TLS extension.  It
		## indicates the server name that the client was requesting.
		server_name:      string           &log &optional;
		## Session ID offered by the client for session resumption.
		## Not used for logging.
		session_id:       string           &optional;
		## Flag to indicate if the session was resumed reusing
		## the key material exchanged in an earlier connection.
		resumed:          bool             &log &default=F;
		## Flag to indicate if we saw a non-empty session ticket being
		## sent by the client using an empty session ID. This value
		## is used to determine if a session is being resumed. It's
		## not logged.
		client_ticket_empty_session_seen: bool &default=F;
		## Flag to indicate if we saw a client key exchange message sent
		## by the client. This value is used to determine if a session
		## is being resumed. It's not logged.
		client_key_exchange_seen: bool     &default=F;
		## Track if the client sent a pre-shared-key extension.
		## Used to determine if a TLS 1.3 session is being resumed.
		## Not logged.
		client_psk_seen: bool     &default=F;

		## Last alert that was seen during the connection.
		last_alert:       string           &log &optional;
		## Next protocol the server chose using the application layer
		## next protocol extension, if present.
		next_protocol:    string           &log &optional;

		## The analyzer ID used for the analyzer instance attached
		## to each connection.  It is not used for logging since it's a
		## meaningless arbitrary number.
		analyzer_id:      count            &optional;

		## Flag to indicate if this ssl session has been established
		## successfully, or if it was aborted during the handshake.
		established:      bool             &log &default=F;
		## Flag to indicate if this record already has been logged, to
		## prevent duplicates.
		logged:           bool             &default=F;

		## SSL history showing which types of packets we received in which order.
		## Letters have the following meaning with client-sent letters being capitalized:
		## H  hello_request
		## C  client_hello
		## S  server_hello
		## V  hello_verify_request
		## T  NewSessionTicket
		## X  certificate
		## K  server_key_exchange
		## R  certificate_request
		## N  server_hello_done
		## Y  certificate_verify
		## G  client_key_exchange
		## F  finished
		## W  certificate_url
		## U  certificate_status
		## A  supplemental_data
		## Z  unassigned_handshake_type
		## I  change_cipher_spec
		## B  heartbeat
		## D  application_data
		## E  end_of_early_data
		## O  encrypted_extensions
		## P  key_update
		## M  message_hash
		## J  hello_retry_request
		## L  alert
		## Q  unknown_content_type
		ssl_history:          string &log &default="";
	};

	## The default root CA bundle.  By default, the mozilla-ca-list.zeek
	## script sets this to Mozilla's root CA list.
	const root_certs: table[string] of string = {} &redef;

	## The record type which contains the field for the Certificate
	## Transparency log bundle.
	type CTInfo: record {
		## Description of the Log
		description:           string;
		## Operator of the Log
		operator:              string;
		## Public key of the Log.
		key:                   string;
		## Maximum merge delay of the Log
		maximum_merge_delay:   count;
		## URL of the Log
		url:                   string;
	};

	## The Certificate Transparency log bundle. By default, the ct-list.zeek
	## script sets this to the current list of known logs. Entries
	## are indexed by (binary) log-id.
	option ct_logs: table[string] of CTInfo = {};

	## If true, detach the SSL analyzer from the connection to prevent
	## continuing to process encrypted traffic. Helps with performance
	## (especially with large file transfers).
	option disable_analyzer_after_detection = T;

	## Delays an SSL record for a specific token: the record will not be
	## logged as long as the token exists or until 15 seconds elapses.
	global delay_log: function(info: Info, token: string);

	## Undelays an SSL record for a previously inserted token, allowing the
	## record to be logged.
	global undelay_log: function(info: Info, token: string);

	## Event that can be handled to access the SSL
	## record as it is sent on to the logging framework.
	global log_ssl: event(rec: Info);

	## Hook that can be used to perform actions right before the log record
	## is written.
	global ssl_finishing: hook(c: connection);

	## SSL finalization hook.  Remaining SSL info may get logged when it's called.
	## The :zeek:see:`SSL::ssl_finishing` hook may either
	## be called before this finalization hook for established SSL connections
	## or during this finalization hook for SSL connections may have info still
	## left to log.
	global finalize_ssl: Conn::RemovalHook;
}

redef record connection += {
	ssl: Info &optional;
};

redef record Info += {
		# Adding a string "token" to this set will cause the SSL script
		# to delay logging the record until either the token has been removed or
		# the record has been delayed.
		delay_tokens: set[string] &optional;
};

const ssl_ports = {
	443/tcp, 563/tcp, 585/tcp, 614/tcp, 636/tcp,
	989/tcp, 990/tcp, 992/tcp, 993/tcp, 995/tcp, 5223/tcp
};

# There are no well known DTLS ports at the moment. Let's
# just add 443 for now for good measure - who knows :)
const dtls_ports = { 443/udp };

redef likely_server_ports += { ssl_ports, dtls_ports };

# Priority needs to be higher than priority of zeek_init in ssl/files.zeek
event zeek_init() &priority=6
	{
	Log::create_stream(SSL::LOG, [$columns=Info, $ev=log_ssl, $path="ssl", $policy=log_policy]);
	Analyzer::register_for_ports(Analyzer::ANALYZER_SSL, ssl_ports);
	Analyzer::register_for_ports(Analyzer::ANALYZER_DTLS, dtls_ports);
	}

function set_session(c: connection)
	{
	if ( ! c?$ssl )
		{
		c$ssl = [$ts=network_time(), $uid=c$uid, $id=c$id];
		Conn::register_removal_hook(c, finalize_ssl);
		}
	}

function add_to_history(c: connection, is_orig: bool, char: string)
	{
	if ( is_orig )
		c$ssl$ssl_history = c$ssl$ssl_history+to_upper(char);
	else
		c$ssl$ssl_history = c$ssl$ssl_history+to_lower(char);
	}

function delay_log(info: Info, token: string)
	{
	if ( ! info?$delay_tokens )
		info$delay_tokens = set();
	add info$delay_tokens[token];
	}

function undelay_log(info: Info, token: string)
	{
	if ( info?$delay_tokens && token in info$delay_tokens )
		delete info$delay_tokens[token];
	}

function log_record(info: Info)
	{
	if ( info$logged )
		return;

	if ( ! info?$delay_tokens || |info$delay_tokens| == 0 )
		{
		Log::write(SSL::LOG, info);
		info$logged = T;
		}
	else
		{
		when ( |info$delay_tokens| == 0 )
			{
			log_record(info);
			}
		timeout 15secs
			{
			# We are just going to log the record anyway.
			delete info$delay_tokens;
			log_record(info);
			}
		}
	}

# remove_analyzer flag is used to prevent disabling analyzer for finished
# connections.
function finish(c: connection, remove_analyzer: bool)
	{
	log_record(c$ssl);
	if ( remove_analyzer && disable_analyzer_after_detection && c?$ssl && c$ssl?$analyzer_id )
		{
		disable_analyzer(c$id, c$ssl$analyzer_id);
		delete c$ssl$analyzer_id;
		}
	}

event ssl_client_hello(c: connection, version: count, record_version: count, possible_ts: time, client_random: string, session_id: string, ciphers: index_vec, comp_methods: index_vec) &priority=5
	{
	set_session(c);

	# Save the session_id if there is one set.
	if ( |session_id| > 0 && session_id != /^\x00{32}$/ )
		{
		c$ssl$session_id = bytestring_to_hexstr(session_id);
		c$ssl$client_ticket_empty_session_seen = F;
		}
	}

event ssl_server_hello(c: connection, version: count, record_version: count, possible_ts: time, server_random: string, session_id: string, cipher: count, comp_method: count) &priority=5
	{
	set_session(c);

	# If it is already filled, we saw a supported_versions extensions which overrides this.
	if ( ! c$ssl?$version_num )
		{
		c$ssl$version_num = version;
		c$ssl$version = version_strings[version];
		}
	c$ssl$cipher = cipher_desc[cipher];

	if ( c$ssl?$session_id && c$ssl$session_id == bytestring_to_hexstr(session_id) && c$ssl$version_num/0xFF != 0x7F && c$ssl$version_num != TLSv13 )
		c$ssl$resumed = T;
	}

event ssl_extension_supported_versions(c: connection, is_orig: bool, versions: index_vec)
	{
	if ( is_orig || |versions| != 1 )
		return;

	set_session(c);

	c$ssl$version_num = versions[0];
	c$ssl$version = version_strings[versions[0]];
	}

event ssl_ecdh_server_params(c: connection, curve: count, point: string) &priority=5
	{
	set_session(c);

	c$ssl$curve = ec_curves[curve];
	}

event ssl_extension_key_share(c: connection, is_orig: bool, curves: index_vec)
	{
	if ( is_orig || |curves| != 1 )
		return;

	set_session(c);
	c$ssl$curve = ec_curves[curves[0]];
	}

event ssl_extension_server_name(c: connection, is_orig: bool, names: string_vec) &priority=5
	{
	set_session(c);

	if ( is_orig && |names| > 0 )
		{
		c$ssl$server_name = names[0];
		if ( |names| > 1 )
			Reporter::conn_weird("SSL_many_server_names", c, cat(names));
		}
	}

event ssl_extension_application_layer_protocol_negotiation(c: connection, is_orig: bool, protocols: string_vec)
	{
	set_session(c);

	if ( is_orig )
		return;

	if ( |protocols| > 0 )
		c$ssl$next_protocol = protocols[0];
	}

event ssl_handshake_message(c: connection, is_orig: bool, msg_type: count, length: count) &priority=5
	{
	set_session(c);

	if ( is_orig && msg_type == SSL::CLIENT_KEY_EXCHANGE )
		c$ssl$client_key_exchange_seen = T;

	switch ( msg_type )
		{
		case SSL::HELLO_REQUEST:
			add_to_history(c, is_orig, "h");
			break;
		case SSL::CLIENT_HELLO:
			add_to_history(c, is_orig, "c");
			break;
		case SSL::SERVER_HELLO:
			add_to_history(c, is_orig, "s");
			break;
		case SSL::HELLO_VERIFY_REQUEST:
			add_to_history(c, is_orig, "v");
			break;
		case SSL::SESSION_TICKET:
			add_to_history(c, is_orig, "t");
			break;
		# end of early data
		case 5:
			add_to_history(c, is_orig, "e");
			break;
		case SSL::HELLO_RETRY_REQUEST:
			add_to_history(c, is_orig, "j");
			break;
		case SSL::ENCRYPTED_EXTENSIONS:
			add_to_history(c, is_orig, "o");
			break;
		case SSL::CERTIFICATE:
			add_to_history(c, is_orig, "x");
			break;
		case SSL::SERVER_KEY_EXCHANGE:
			add_to_history(c, is_orig, "k");
			break;
		case SSL::CERTIFICATE_REQUEST:
			add_to_history(c, is_orig, "r");
			break;
		case SSL::SERVER_HELLO_DONE:
			add_to_history(c, is_orig, "n");
			break;
		case SSL::CERTIFICATE_VERIFY:
			add_to_history(c, is_orig, "y");
			break;
		case SSL::CLIENT_KEY_EXCHANGE:
			add_to_history(c, is_orig, "g");
			break;
		case SSL::FINISHED:
			add_to_history(c, is_orig, "f");
			break;
		case SSL::CERTIFICATE_URL:
			add_to_history(c, is_orig, "w");
			break;
		case SSL::CERTIFICATE_STATUS:
			add_to_history(c, is_orig, "u");
			break;
		case SSL::SUPPLEMENTAL_DATA:
			add_to_history(c, is_orig, "a");
			break;
		case SSL::KEY_UPDATE:
			add_to_history(c, is_orig, "p");
			break;
		# message hash
		case 254:
			add_to_history(c, is_orig, "m");
			break;
		default:
			add_to_history(c, is_orig, "z");
			break;
		}
	}

# Extension event is fired _before_ the respective client or server hello.
# Important for client_ticket_empty_session_seen.
event ssl_extension(c: connection, is_orig: bool, code: count, val: string) &priority=5
	{
	set_session(c);

	if ( is_orig && code == SSL_EXTENSION_SESSIONTICKET_TLS && |val| > 0 )
		# In this case, we might have an empty ID. Set back to F in client_hello event
		# if it is not empty after all.
		c$ssl$client_ticket_empty_session_seen = T;
	else if ( is_orig && code == SSL_EXTENSION_PRE_SHARED_KEY )
		# In this case, the client sent a PSK extension which can be used for resumption
		c$ssl$client_psk_seen = T;
	else if ( ! is_orig && code == SSL_EXTENSION_PRE_SHARED_KEY && c$ssl$client_psk_seen )
		# In this case, the server accepted the PSK offered by the client.
		c$ssl$resumed = T;
	}

event ssl_change_cipher_spec(c: connection, is_orig: bool) &priority=5
	{
	set_session(c);
	add_to_history(c, is_orig, "i");

	if ( is_orig && c$ssl$client_ticket_empty_session_seen && ! c$ssl$client_key_exchange_seen )
		c$ssl$resumed = T;
	}

event ssl_alert(c: connection, is_orig: bool, level: count, desc: count) &priority=5
	{
	set_session(c);
	add_to_history(c, is_orig, "l");

	c$ssl$last_alert = alert_descriptions[desc];
	}

event ssl_heartbeat(c: connection, is_orig: bool, length: count, heartbeat_type: count, payload_length: count, payload: string)
	{
	set_session(c);
	add_to_history(c, is_orig, "b");
	}

event ssl_established(c: connection) &priority=7
	{
	c$ssl$established = T;
	}

event ssl_established(c: connection) &priority=20
	{
	set_session(c);
	hook ssl_finishing(c);
	}

event ssl_established(c: connection) &priority=-5
	{
	finish(c, T);
	}

hook finalize_ssl(c: connection)
	{
	if ( ! c?$ssl )
		return;

	if ( ! c$ssl$logged )
		hook ssl_finishing(c);

	# called in case a SSL connection that has not been established terminates
	finish(c, F);
	}

event protocol_confirmation(c: connection, atype: Analyzer::Tag, aid: count) &priority=5
	{
	if ( atype == Analyzer::ANALYZER_SSL || atype == Analyzer::ANALYZER_DTLS )
		{
		set_session(c);
		c$ssl$analyzer_id = aid;
		}
	}

event ssl_plaintext_data(c: connection, is_orig: bool, record_version: count, content_type: count, length: count) &priority=5
	{
	set_session(c);

	if ( ! c$ssl?$version || c$ssl$established || content_type != APPLICATION_DATA )
		return;

	local wi = Weird::Info($ts=network_time(), $name="ssl_early_application_data", $uid=c$uid, $id=c$id);
	Weird::weird(wi);
	}

event protocol_violation(c: connection, atype: Analyzer::Tag, aid: count,
                         reason: string) &priority=5
	{
	if ( c?$ssl && ( atype == Analyzer::ANALYZER_SSL || atype == Analyzer::ANALYZER_DTLS ) )
		finish(c, T);
	}
