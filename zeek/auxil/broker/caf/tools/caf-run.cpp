// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "caf/all.hpp"
#include "caf/detail/encode_base64.hpp"
#include "caf/io/all.hpp"

using namespace caf;

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;


class host_desc {
public:
  std::string host;
  int cpu_slots;

  host_desc(std::string host_addr, int slots)
    : host(std::move(host_addr)), cpu_slots(slots) {
    // nop
  }

  host_desc(host_desc&&) = default;
  host_desc(const host_desc&) = default;
  host_desc& operator=(host_desc&&) = default;
  host_desc& operator=(const host_desc&) = default;

  static void append(vector<host_desc>& xs, const string& line) {
    vector<string> fields;
    split(fields, line, is_any_of(" "), token_compress_on);
    if (fields.empty())
      return;
    host_desc hd;
    hd.host = std::move(fields.front());
    hd.cpu_slots = 0;
    xs.emplace_back(std::move(hd));
  }

private:
  host_desc() = default;
};

std::vector<host_desc> read_hostfile(const string& fname) {
  std::vector<host_desc> result;
  std::ifstream in{fname};
  std::string line;
  while (std::getline(in, line))
    host_desc::append(result, line);
  return result;
}

bool run_ssh(actor_system& system, const string& wdir, const string& cmd,
             const string& host) {
  std::cout << "runssh, wdir: " << wdir << " cmd: " << cmd << " host: " << host
            << std::endl;
  // pack command before sending it to avoid any issue with shell escaping
  string full_cmd = "cd ";
  full_cmd += wdir;
  full_cmd += '\n';
  full_cmd += cmd;
  auto packed = detail::base64::encode(full_cmd);
  std::ostringstream oss;
  oss << "ssh -Y -o ServerAliveInterval=60 " << host << R"( "echo )" << packed
      << R"( | base64 --decode | /bin/sh")";
  // return system(oss.str().c_str());
  string line;
  std::cout << "popen: " << oss.str() << std::endl;
  auto fp = popen(oss.str().c_str(), "r");
  if (fp == nullptr)
    return false;
  char buf[512];
  auto eob = buf + sizeof(buf); // end-of-buf
  auto pred = [](char c) { return c == 0 || c == '\n'; };
  scoped_actor self{system};
  while (fgets(buf, sizeof(buf), fp) != nullptr) {
    auto i = buf;
    auto e = std::find_if(i, eob, pred);
    line.insert(line.end(), i, e);
    while (e != eob && *e != 0) {
      aout(self) << line << std::endl;
      line.clear();
      i = e + 1;
      e = std::find_if(i, eob, pred);
      line.insert(line.end(), i, e);
    }
  }
  pclose(fp);
  std::cout << "host down: " << host << std::endl;
  if (!line.empty())
    aout(self) << line << std::endl;
  return true;
}

void bootstrap(actor_system& system, const string& wdir,
               const host_desc& master, vector<host_desc> slaves,
               const string& cmd, vector<string> args) {
  using io::network::interfaces;
  if (!args.empty())
    args.erase(args.begin());
  scoped_actor self{system};
  // open a random port and generate a list of all
  // possible addresses slaves can use to connect to us
  auto port_res = system.middleman().publish(self, 0);
  if (!port_res) {
    cerr << "fatal: unable to publish actor: " << to_string(port_res.error())
         << endl;
    return;
  }
  auto port = *port_res;
  // run a slave process at master host if user defined slots > 1 for it
  if (master.cpu_slots > 1)
    slaves.emplace_back(master.host, master.cpu_slots - 1);
  for (auto& slave : slaves) {
    using namespace caf::io::network;
    // build SSH command and pack it to avoid any issue with shell escaping
    std::thread{[=, &system](actor bootstrapper) {
                  std::ostringstream oss;
                  oss << cmd;
                  if (slave.cpu_slots > 0)
                    oss << " --caf.scheduler.max-threads=" << slave.cpu_slots;
                  oss << " --caf.slave-mode"
                      << " --caf.slave-name=" << slave.host
                      << " --caf.bootstrap-node=";
                  bool is_first = true;
                  interfaces::traverse({protocol::ipv4, protocol::ipv6},
                                       [&](const char*, protocol::network,
                                           bool lo, const char* x) {
                                         if (lo)
                                           return;
                                         if (!is_first)
                                           oss << ",";
                                         else
                                           is_first = false;
                                         oss << x << "/" << port;
                                       });
                  for (auto& arg : args)
                    oss << " " << arg;
                  if (!run_ssh(system, wdir, oss.str(), slave.host))
                    anon_send(bootstrapper, slave.host);
                },
                actor{self}}
      .detach();
  }
  std::string slaveslist;
  for (size_t i = 0; i < slaves.size(); ++i) {
    self->receive(
      [&](const string& host, uint16_t slave_port) {
        if (!slaveslist.empty())
          slaveslist += ',';
        slaveslist += host;
        slaveslist += '/';
        slaveslist += std::to_string(slave_port);
      },
      [](const string& node) {
        cerr << "unable to launch process via SSH at node " << node << endl;
      });
  }
  // run (and wait for) master
  std::ostringstream oss;
  oss << cmd << " --caf.slave-nodes=" << slaveslist << " " << join(args, " ");
  run_ssh(system, wdir, oss.str(), master.host);
}

#define RETURN_WITH_ERROR(output)                                              \
  do {                                                                         \
    ::std::cerr << output << ::std::endl;                                      \
    return EXIT_FAILURE;                                                       \
  } while (true)

namespace {

struct config : actor_system_config {
  config() {
    opt_group{custom_options_, "global"}
      .add(hostfile, "hostfile", "path to hostfile")
      .add(wdir, "wdir", "working directory");
  }
  string hostfile;
  string wdir;
};

} // namespace

int main(int argc, char** argv) {
  config cfg;
  if (auto err = cfg.parse(argc, argv)) {
    std::cerr << "error parsing command line: " << to_string(err) << '\n';
    return EXIT_FAILURE;
  }
  if (cfg.cli_helptext_printed)
    return EXIT_SUCCESS;
  if (cfg.slave_mode)
    RETURN_WITH_ERROR("cannot use slave mode in caf-run tool");
  std::unique_ptr<char, void (*)(void*)> pwd{getcwd(nullptr, 0), ::free};
  if (cfg.hostfile.empty())
    RETURN_WITH_ERROR("no hostfile specified or hostfile is empty");
  auto& remainder = cfg.remainder;
  if (remainder.empty())
    RETURN_WITH_ERROR("empty command line");
  auto cmd = std::move(remainder.front());
  vector<string> xs;
  for (auto i = cfg.remainder.begin() + 1; i != cfg.remainder.end(); ++i)
    xs.emplace_back(std::move(*i));
  auto hosts = read_hostfile(cfg.hostfile);
  if (hosts.empty())
    RETURN_WITH_ERROR("no valid entry in hostfile");
  actor_system system{cfg};
  auto master = hosts.front();
  hosts.erase(hosts.begin());
  bootstrap(system, (cfg.wdir.empty()) ? pwd.get() : cfg.wdir.c_str(), master,
            std::move(hosts), cmd, xs);
  return EXIT_SUCCESS;
}
