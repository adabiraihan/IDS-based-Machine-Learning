/******************************************************************************\
 * This example program represents a minimal GUI chat program                 *
 * based on group communication. This chat program is compatible to the       *
 * terminal version in remote_actors/group_chat.cpp.                          *
 *                                                                            *
 * Setup for a minimal chat between "alice" and "bob":                        *
 * - group_server -p 4242                                                     *
 * - qt_group_chat -g remote:chatroom@localhost:4242 -n alice                 *
 * - qt_group_chat -g remote:chatroom@localhost:4242 -n bob                   *
\******************************************************************************/

#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <time.h>
#include <vector>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

CAF_PUSH_WARNINGS
#include "ui_chatwindow.h" // auto generated from chatwindow.ui
#include <QApplication>
#include <QMainWindow>
CAF_POP_WARNINGS

#include "chatwidget.hpp"

using namespace caf;

class config : public actor_system_config {
public:
  config() {
    opt_group{custom_options_, "global"}
      .add<std::string>("name,n", "set name")
      .add<std::string>("group,g", "join group");
  }
};

int caf_main(actor_system& sys, const config& cfg) {
  std::string name;
  if (auto config_name = get_if<std::string>(&cfg, "name"))
    name = std::move(*config_name);
  while (name.empty()) {
    std::cout << "please enter your name: " << std::flush;
    if (!std::getline(std::cin, name)) {
      std::cerr << "*** no name given... terminating" << std::endl;
      return EXIT_FAILURE;
    }
  }
  group grp;
  // evaluate group parameters
  if (auto locator = get_if<std::string>(&cfg, "group")) {
    if (auto maybe_grp = sys.groups().get(*locator)) {
      grp = std::move(*maybe_grp);
    } else {
      std::cerr << R"(*** failed to parse ")" << *locator
                << R"(" as group locator: )" << to_string(maybe_grp.error())
                << std::endl;
    }
  }
  auto [argc, argv] = cfg.c_args_remainder();
  QApplication app{argc, argv};
  app.setQuitOnLastWindowClosed(true);
  QMainWindow mw;
  Ui::ChatWindow helper;
  helper.setupUi(&mw);
  helper.chatwidget->init(sys);
  auto client = helper.chatwidget->as_actor();
  anon_send(client, set_name_atom_v, move(name));
  anon_send(client, join_atom_v, std::move(grp));
  mw.show();
  return app.exec();
}

CAF_MAIN(id_block::qtsupport, io::middleman)
