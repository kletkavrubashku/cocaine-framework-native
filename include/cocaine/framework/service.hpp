/*
    Copyright (c) 2015 Evgeny Safronov <division494@gmail.com>
    Copyright (c) 2011-2015 Other contributors as noted in the AUTHORS file.
    This file is part of Cocaine.
    Cocaine is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
    Cocaine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "cocaine/framework/forwards.hpp"
#include "cocaine/framework/receiver.hpp"
#include "cocaine/framework/session.hpp"
#include "cocaine/framework/scheduler.hpp"

namespace cocaine {

namespace framework {

/// The invocation result helper trait allows you to unpack invocation result to something more
/// convenient.
///
/// Must provide the type typedef for result type.
///
/// \helper
template<
    class Event,
    class Upstream = typename io::event_traits<Event>::upstream_type,
    class Dispatch = typename io::event_traits<Event>::dispatch_type
>
struct invocation_result {
    typedef std::tuple<
        sender  <Dispatch, basic_session_t>,
        receiver<Upstream, basic_session_t>
    > channel_type;

    typedef channel_type type;

    static
    typename task<type>::future_type
    apply(channel_type&& channel) {
        return make_ready_future<type>::value(std::move(channel));
    }
};

/// The template trait specialization for primitive tags with no sender.
///
/// On apply unpacks the single option value and returns it on value type, throws otherwise.
///
/// \helper
template<class Event, class T>
struct invocation_result<Event, io::primitive_tag<T>, void> {
    typedef std::tuple<
        sender  <void, basic_session_t>,
        receiver<io::primitive_tag<T>, basic_session_t>
    > channel_type;

    typedef typename detail::packable<T>::type type;

    static
    typename task<type>::future_type
    apply(channel_type&& channel) {
        auto rx = std::move(std::get<1>(channel));
        return rx.recv();
    }
};

class basic_service_t {
public:
    typedef std::vector<session_t::endpoint_type> endpoints_t;

private:
    class impl;
    std::unique_ptr<impl> d;
    std::shared_ptr<session_t> session;
    scheduler_t& scheduler;

public:
    basic_service_t(std::string name, uint version, endpoints_t locations, scheduler_t& scheduler);
    basic_service_t(basic_service_t&& other);

    ~basic_service_t();

    auto name() const noexcept -> const std::string&;
    auto version() const noexcept -> uint;

    auto connect() -> task<void>::future_type;

    auto endpoint() const -> boost::optional<session_t::endpoint_type>;

    template<class Event, class... Args>
    typename task<typename invocation_result<Event>::type>::future_type
    invoke(Args&&... args) {
        namespace ph = std::placeholders;

        context_holder holder("`" + name() + "|SI");
        return connect()
            .then(scheduler, wrap(std::bind(&basic_service_t::on_connect<Event, Args...>, ph::_1, session, std::forward<Args>(args)...)))
            .then(scheduler, wrap(std::bind(&basic_service_t::on_invoke<Event>, ph::_1)));
    }

private:
    template<class Event, class... Args>
    static
    typename task<typename session_t::invoke_result<Event>::type>::future_type
    on_connect(typename task<void>::future_type& f, std::shared_ptr<session_t> session, Args&... args) {
        f.get();
        // Between these calls no one can guarantee, that the connection won't be broken. In this
        // case you will get a system error after either write or read attempt.
        return session->invoke<Event>(std::forward<Args>(args)...);
    }

    template<class Event>
    static
    typename task<typename invocation_result<Event>::type>::future_type
    on_invoke(typename task<typename session_t::invoke_result<Event>::type>::future_move_type future) {
        return invocation_result<Event>::apply(future.get());
    }
};

template<class T>
class service : public basic_service_t {
public:
    service(std::string name, endpoints_t locations, scheduler_t& scheduler) :
        basic_service_t(std::move(name), io::protocol<T>::version::value, std::move(locations), scheduler)
    {}
};

} // namespace framework

} // namespace cocaine
