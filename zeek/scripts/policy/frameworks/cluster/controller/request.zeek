@load ./types

module ClusterController::Request;

export {
	type Request: record {
		id: string;
		parent_id: string &optional;
	};

	# API-specific state. XXX we may be able to generalize after this
	# has settled a bit more.

	# State specific to the set_configuration request/response events
	type SetConfigurationState: record {
		requests: vector of Request &default=vector();
	};

	# State specific to the set_nodes request/response events
	type SetNodesState: record {
		requests: vector of Request &default=vector();
	};

	# State specific to supervisor interactions
	type SupervisorState: record {
		node: string;
	};

	# The redef is a workaround so we can use the Request type
        # while it is still being defined
	redef record Request += {
		results: ClusterController::Types::ResultVec &default=vector();
		finished: bool &default=F;

		set_configuration_state: SetConfigurationState &optional;
		set_nodes_state: SetNodesState &optional;
		supervisor_state: SupervisorState &optional;
	};

	global null_req = Request($id="", $finished=T);

	global create: function(reqid: string &default=unique_id("")): Request;
	global lookup: function(reqid: string): Request;
	global finish: function(reqid: string): bool;

	global is_null: function(request: Request): bool;
}

# XXX this needs a mechanism for expiring stale requests
global requests: table[string] of Request;

function create(reqid: string): Request
	{
	local ret = Request($id=reqid);
	requests[reqid] = ret;
	return ret;
	}

function lookup(reqid: string): Request
	{
	if ( reqid in requests )
		return requests[reqid];

	return null_req;
	}

function finish(reqid: string): bool
	{
	if ( reqid !in requests )
		return F;

	local req = requests[reqid];
	delete requests[reqid];

	req$finished = T;

	return T;
	}

function is_null(request: Request): bool
	{
	if ( request$id == "" )
		return T;

	return F;
	}
