// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE io.broker

#include "caf/io/broker.hpp"

#include "caf/test/io_dsl.hpp"

#include <cstdint>
#include <iostream>
#include <memory>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace caf;
using namespace caf::io;

namespace {

struct suite_state {
  int pings = 0;
  int pongs = 0;
  suite_state() = default;
};

using suite_state_ptr = std::shared_ptr<suite_state>;

behavior ping(event_based_actor* self, suite_state_ptr ssp) {
  return {
    [=](ok_atom, const actor& pong) {
      CAF_MESSAGE("received `ok_atom`");
      ++ssp->pings;
      self->send(pong, ping_atom_v);
      self->become({
        [=](pong_atom) {
          CAF_MESSAGE("ping: received pong");
          self->send(pong, ping_atom_v);
          if (++ssp->pings == 10) {
            self->quit();
            CAF_MESSAGE("ping is done");
          }
        },
        [=](ping_atom) { CAF_FAIL("ping received a ping message"); },
      });
    },
  };
}

behavior pong(event_based_actor* self, suite_state_ptr ssp) {
  return {
    [=](ping_atom) -> pong_atom {
      CAF_MESSAGE("pong: received ping");
      if (++ssp->pongs == 10) {
        self->quit();
        CAF_MESSAGE("pong is done");
      }
      return pong_atom_v;
    },
  };
}

behavior peer_fun(broker* self, connection_handle hdl, const actor& buddy) {
  CAF_MESSAGE("peer_fun called");
  CAF_REQUIRE(self != nullptr);
  CAF_REQUIRE(self->subtype() == resumable::io_actor);
  self->monitor(buddy);
  self->set_down_handler([self](down_msg& dm) {
    // Stop if buddy is done.
    self->quit(std::move(dm.reason));
  });
  // Assume exactly one connection.
  CAF_REQUIRE(self->connections().size() == 1);
  self->configure_read(hdl, receive_policy::exactly(sizeof(type_id_t)));
  auto write = [=](type_id_t type) {
    auto& buf = self->wr_buf(hdl);
    auto first = reinterpret_cast<byte*>(&type);
    buf.insert(buf.end(), first, first + sizeof(type_id_t));
    self->flush(hdl);
  };
  return {
    [=](const connection_closed_msg&) {
      CAF_MESSAGE("received connection_closed_msg");
      self->quit();
    },
    [=](const new_data_msg& msg) {
      CAF_MESSAGE("received new_data_msg");
      CAF_REQUIRE_EQUAL(msg.buf.size(), sizeof(type_id_t));
      type_id_t type = 0;
      memcpy(&type, msg.buf.data(), sizeof(type_id_t));
      static_assert(type_id_v<ping_atom> != type_id_v<pong_atom>);
      if (type == type_id_v<ping_atom>) {
        self->send(buddy, ping_atom_v);
      } else if (type == type_id_v<pong_atom>) {
        self->send(buddy, pong_atom_v);
      } else {
        CAF_FAIL("unexpected message type");
      }
    },
    [=](ping_atom) { write(type_id_v<ping_atom>); },
    [=](pong_atom) { write(type_id_v<pong_atom>); },
  };
}

behavior peer_acceptor_fun(broker* self, const actor& buddy) {
  CAF_MESSAGE("peer_acceptor_fun");
  return {
    [=](const new_connection_msg& msg) {
      CAF_MESSAGE("received `new_connection_msg`");
      self->fork(peer_fun, msg.handle, buddy);
      self->quit();
    },
    [=](publish_atom) -> result<uint16_t> {
      if (auto res = self->add_tcp_doorman(8080))
        return res->second;
      else
        return std::move(res.error());
    },
  };
}

using int_peer = connection_handler::extend<replies_to<int>::with<int>>;

int_peer::behavior_type int_peer_fun(int_peer::broker_pointer) {
  return {
    [=](const connection_closed_msg&) {
      CAF_FAIL("received connection_closed_msg");
    },
    [=](const new_data_msg&) { CAF_FAIL("received new_data_msg"); },
    [=](int value) {
      CAF_MESSAGE("received: " << value);
      return value;
    },
  };
}

} // namespace

CAF_TEST_FIXTURE_SCOPE(broker_tests, point_to_point_fixture<>)

CAF_TEST(test broker to broker communication) {
  prepare_connection(mars, earth, "mars", 8080);
  CAF_MESSAGE("spawn peer acceptor on mars");
  auto ssp = std::make_shared<suite_state>();
  auto server = mars.mm.spawn_broker(peer_acceptor_fun,
                                     mars.sys.spawn(pong, ssp));
  mars.self->send(server, publish_atom_v);
  run();
  expect_on(mars, (uint16_t), from(server).to(mars.self).with(8080));
  CAF_MESSAGE("spawn ping and client on earth");
  auto pinger = earth.sys.spawn(ping, ssp);
  auto client = unbox(earth.mm.spawn_client(peer_fun, "mars", 8080, pinger));
  anon_send(pinger, ok_atom_v, client);
  run();
  CAF_CHECK_EQUAL(ssp->pings, 10);
  CAF_CHECK_EQUAL(ssp->pongs, 10);
}

CAF_TEST(test whether we can spawn typed broker) {
  auto peer = mars.mm.spawn_broker(int_peer_fun);
  mars.self->send(peer, 42);
  run();
  expect_on(mars, (int), from(peer).to(mars.self).with(42));
}

CAF_TEST_FIXTURE_SCOPE_END()
