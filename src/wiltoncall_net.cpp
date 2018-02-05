/* 
 * File:   wiltoncall_net.cpp
 * Author: alex
 *
 * Created on October 17, 2017, 8:59 PM
 */

#include "wilton/wilton_net.h"

#include "staticlib/config.hpp"
#include "staticlib/json.hpp"
#include "staticlib/utils.hpp"

#include "wilton/support/handle_registry.hpp"
#include "wilton/support/buffer.hpp"
#include "wilton/support/exception.hpp"
#include "wilton/support/registrar.hpp"

namespace wilton {
namespace net {

namespace { //anonymous

std::shared_ptr<support::handle_registry<wilton_socket_handler>> shared_socket_registry() {
    static auto registry = std::make_shared<support::handle_registry<wilton_socket_handler>>(
        [] (wilton_socket_handler* conn) STATICLIB_NOEXCEPT {
            wilton_net_socket_close(conn);
        });
    return registry;
}

}

support::buffer wait_for_tcp_connection(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t timeout = -1;
    auto rip = std::ref(sl::utils::empty_string());
    int64_t port = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("ipAddress" == name) {
            rip = fi.as_string_nonempty_or_throw(name);
        } else if ("tcpPort" == name) {
            port = fi.as_int64_or_throw(name);
        } else if ("timeoutMillis" == name) {
            timeout = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == timeout) throw support::exception(TRACEMSG(
            "Required parameter 'timeoutMillis' not specified"));
    if (rip.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'ipAddress' not specified"));
    if (-1 == port) throw support::exception(TRACEMSG(
            "Required parameter 'tcpPort' not specified"));
    const std::string& ip = rip.get();
    // call wilton
    char* err = wilton_net_wait_for_tcp_connection(ip.c_str(), static_cast<int>(ip.length()),
            static_cast<int> (port), static_cast<int> (timeout));
    if (nullptr != err) {
        support::throw_wilton_error(err, TRACEMSG(err));
    }
    return support::make_empty_buffer();
}


// ASIO TCP/UDP API

// socket_open
support::buffer socket_open(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t timeout = -1;
    auto rip = std::ref(sl::utils::empty_string()); // ref to ip string
    int64_t port = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("ipAddress" == name) {
            rip = fi.as_string_nonempty_or_throw(name);
        } else if ("tcpPort" == name) {
            port = fi.as_int64_or_throw(name);
        } else if ("timeoutMillis" == name) {
            timeout = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == timeout) throw support::exception(TRACEMSG(
            "Required parameter 'timeoutMillis' not specified"));
    if (rip.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'ipAddress' not specified"));
    if (-1 == port) throw support::exception(TRACEMSG(
            "Required parameter 'tcpPort' not specified"));
    const std::string& ip = rip.get();

    wilton_socket_handler* socket;
    char* err = wilton_net_socket_open(std::addressof(socket), ip.c_str(), static_cast<int>(ip.length()),
            static_cast<int> (port), static_cast<int> (timeout));
    if (nullptr != err) {
        support::throw_wilton_error(err, TRACEMSG(err));
    }
    auto reg = shared_socket_registry();
    int64_t handle = reg->put(socket);

    return support::make_json_buffer({
        { "connectionHandle", handle}
    });
} 


support::buffer socket_close(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("connectionHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'connectionHandle' not specified"));
    // get handle
    auto reg = shared_socket_registry();
    wilton_socket_handler* socket = reg->remove(handle);
    if (nullptr == socket) throw support::exception(TRACEMSG(
            "Invalid 'connectionHandle' parameter specified"));
    char* err = wilton_net_socket_close(socket);
//    reg->put(socket); // возможно стоит удалить тут обработчик сокета а не вставлять его занова
    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err +
            "\nwilton_net_socket_close error for input data"));

    return support::make_empty_buffer();
}

support::buffer socket_write(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
    auto req_message_data = std::ref(sl::utils::empty_string());
//    int32_t data_len = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("connectionHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else if ("message_data" == name) {
            req_message_data = fi.as_string_nonempty_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    // check json data
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'connectionHandle' not specified"));
    if (req_message_data.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'message_data' not specified"));
    const std::string& message_data = req_message_data.get();

    // get handle
    auto reg = shared_socket_registry();
    wilton_socket_handler* socket = reg->remove(handle);
    if (nullptr == socket) throw support::exception(TRACEMSG(
            "Invalid 'connectionHandle' parameter specified"));
    // call wilton
//    char* out = nullptr;
//    int out_len = 0;
    char* err = wilton_net_socket_write(socket, message_data.c_str(), static_cast<int>(message_data.length()));
    reg->put(socket);

    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));
//    return support::wrap_wilton_buffer(out, out_len);

    return support::make_empty_buffer();
}

support::buffer socket_read(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    int64_t handle = -1;
//    int32_t data_len = -1;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("connectionHandle" == name) {
            handle = fi.as_int64_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    // check json data
    if (-1 == handle) throw support::exception(TRACEMSG(
            "Required parameter 'connectionHandle' not specified"));

    // get handle
    auto reg = shared_socket_registry();
    wilton_socket_handler* socket = reg->remove(handle);
    if (nullptr == socket) throw support::exception(TRACEMSG(
            "Invalid 'connectionHandle' parameter specified"));
    // call wilton
    char* out = nullptr;
    int out_len = 0;
    char* err = wilton_net_socket_read(socket, out, out_len);
    reg->put(socket);

    if (nullptr != err) support::throw_wilton_error(err, TRACEMSG(err));

    return support::wrap_wilton_buffer(out, out_len);
}

} // namespace
}

extern "C" char* wilton_module_init() {
    try {
        wilton::support::register_wiltoncall("net_wait_for_tcp_connection", wilton::net::wait_for_tcp_connection);
        wilton::support::register_wiltoncall("net_socket_open",  wilton::net::socket_open);
        wilton::support::register_wiltoncall("net_socket_close", wilton::net::socket_close);
        wilton::support::register_wiltoncall("net_socket_write", wilton::net::socket_write);
        wilton::support::register_wiltoncall("net_socket_read",  wilton::net::socket_read);

        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}
