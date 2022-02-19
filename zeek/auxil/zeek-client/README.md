# The Zeek Cluster Management Client

This is an early, experimental version of the future client application for
managing Zeek clients. `zeek-client` connects to Zeek's _cluster controller_,
of which there is one in every cluster. The controller in turn is connected
to the cluster's _instances_, physical machines each running an _agent_ that
maintains the _data nodes_ composing a typical Zeek cluster (with manager,
workers, proxies, and loggers).
