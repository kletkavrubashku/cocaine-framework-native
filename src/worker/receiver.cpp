#include "cocaine/framework/worker/receiver.hpp"

#include <cocaine/traits/tuple.hpp>

#include <cocaine/detail/service/node/messages.hpp>

#include "cocaine/framework/message.hpp"
#include "cocaine/framework/receiver.hpp"
#include "cocaine/framework/worker/error.hpp"

namespace ph = std::placeholders;

using namespace cocaine;
using namespace cocaine::framework;
using namespace cocaine::framework::worker;

namespace {

boost::optional<std::string>
on_recv(typename task<decoded_message>::future_move_type future) {
    const auto message = future.get();
    const auto id = message.type();
    switch (id) {
    case io::event_traits<io::rpc::chunk>::id: {
        std::string chunk;
        io::type_traits<
            typename io::event_traits<io::rpc::chunk>::argument_type
        >::unpack(message.args(), chunk);
        return chunk;
    }
    case io::event_traits<io::rpc::error>::id: {
        int id;
        std::string reason;
        io::type_traits<
            typename io::event_traits<io::rpc::error>::argument_type
        >::unpack(message.args(), id, reason);
        throw request_error(id, std::move(reason));
    }
    case io::event_traits<io::rpc::choke>::id:
        return boost::none;
    default:
        throw invalid_protocol_type(id);
    }

    return boost::none;
}

} // namespace

worker::receiver::receiver(std::shared_ptr<basic_receiver_t<worker_session_t>> session) :
    session(std::move(session))
{}

auto worker::receiver::recv() -> typename task<boost::optional<std::string>>::future_type {
    return session->recv()
        .then(std::bind(&on_recv, ph::_1));
}
