#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <array>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

namespace {

constexpr auto operation_deadline = std::chrono::seconds{2};

class AsyncTest {
public:
    template <typename Starter>
    bool run(Starter&& starter)
    {
        deadline_.expires_after(operation_deadline);
        deadline_.async_wait([this](const boost::system::error_code& error) {
            if (!error) {
                fail("operation deadline expired");
            }
        });

        try {
            starter(*this);
            context_.run();
        } catch (const std::exception& exception) {
            fail(std::string{"exception: "} + exception.what());
            context_.run();
        }

        if (!finished_) {
            fail("test finished without a result");
            context_.run();
        }

        if (!failure_.empty()) {
            std::cerr << failure_ << '\n';
            return false;
        }
        return true;
    }

    asio::io_context& context() { return context_; }

    void set_cleanup(std::function<void()> cleanup) { cleanup_ = std::move(cleanup); }

    void pass()
    {
        if (finished_) {
            return;
        }
        finished_ = true;
        cleanup();
    }

    void fail(std::string message)
    {
        if (finished_) {
            return;
        }
        finished_ = true;
        failure_ = std::move(message);
        cleanup();
    }

    bool finished() const { return finished_; }

private:
    void cleanup()
    {
        if (cleanup_) {
            cleanup_();
        }
        deadline_.cancel();
    }

    asio::io_context context_;
    asio::steady_timer deadline_{context_};
    std::function<void()> cleanup_;
    bool finished_{false};
    std::string failure_;
};

struct TcpState {
    explicit TcpState(asio::io_context& context)
        : acceptor(context), server(context), client(context), cancel_timer(context)
    {
    }

    tcp::acceptor acceptor;
    tcp::socket server;
    tcp::socket client;
    asio::steady_timer cancel_timer;
};

void close_socket(tcp::socket& socket)
{
    boost::system::error_code ignored;
    socket.close(ignored);
}

void close_acceptor(tcp::acceptor& acceptor)
{
    boost::system::error_code ignored;
    acceptor.close(ignored);
}

void start_loopback_listener(tcp::acceptor& acceptor)
{
    acceptor.open(tcp::v4());
    acceptor.bind({asio::ip::address_v4::loopback(), 0});
    acceptor.listen();
}

void test_tcp_connect_accept(AsyncTest& test)
{
    auto state = std::make_shared<TcpState>(test.context());
    test.set_cleanup([state] {
        close_acceptor(state->acceptor);
        close_socket(state->server);
        close_socket(state->client);
    });

    start_loopback_listener(state->acceptor);
    const auto endpoint = state->acceptor.local_endpoint();
    auto accepted = std::make_shared<bool>(false);
    auto connected = std::make_shared<bool>(false);
    const auto complete = [&test, accepted, connected] {
        if (*accepted && *connected) {
            test.pass();
        }
    };

    state->acceptor.async_accept(state->server, [state, &test, accepted, complete](const boost::system::error_code& error) {
        if (test.finished()) {
            return;
        }
        if (error) {
            test.fail("accept failed: " + error.message());
            return;
        }
        *accepted = true;
        complete();
    });
    state->client.async_connect(endpoint, [state, &test, connected, complete](const boost::system::error_code& error) {
        if (test.finished()) {
            return;
        }
        if (error) {
            test.fail("connect failed: " + error.message());
            return;
        }
        *connected = true;
        complete();
    });
}

void test_tcp_connection_refused(AsyncTest& test)
{
    struct State {
        explicit State(asio::io_context& context) : reservation(context), client(context) {}
        tcp::acceptor reservation;
        tcp::socket client;
    };

    auto state = std::make_shared<State>(test.context());
    test.set_cleanup([state] {
        close_acceptor(state->reservation);
        close_socket(state->client);
    });

    state->reservation.open(tcp::v4());
    state->reservation.bind({asio::ip::address_v4::loopback(), 0});
    const auto endpoint = state->reservation.local_endpoint();
    close_acceptor(state->reservation);
    state->client.async_connect(endpoint, [state, &test](const boost::system::error_code& error) {
        if (test.finished()) {
            return;
        }
        if (error == asio::error::connection_refused ||
            error == make_error_code(boost::system::errc::connection_refused)) {
            test.pass();
            return;
        }
        if (!error) {
            test.fail("connect unexpectedly succeeded to a non-listening port");
            return;
        }
        test.fail("connect returned unexpected error: " + error.message());
    });
}

void test_tcp_eof_shutdown(AsyncTest& test)
{
    struct State : TcpState {
        explicit State(asio::io_context& context) : TcpState(context) {}
        std::array<char, 32> buffer{};
    };
    auto state = std::make_shared<State>(test.context());
    test.set_cleanup([state] {
        close_acceptor(state->acceptor);
        close_socket(state->server);
        close_socket(state->client);
    });

    start_loopback_listener(state->acceptor);
    const auto endpoint = state->acceptor.local_endpoint();
    state->acceptor.async_accept(state->server, [state, &test](const boost::system::error_code& error) {
        if (test.finished()) {
            return;
        }
        if (error) {
            test.fail("accept failed: " + error.message());
            return;
        }
        boost::system::error_code shutdown_error;
        state->server.shutdown(tcp::socket::shutdown_send, shutdown_error);
        if (shutdown_error) {
            test.fail("server shutdown failed: " + shutdown_error.message());
            return;
        }
        close_socket(state->server);
    });
    state->client.async_connect(endpoint, [state, &test](const boost::system::error_code& error) {
        if (test.finished()) {
            return;
        }
        if (error) {
            test.fail("connect failed: " + error.message());
            return;
        }
        state->client.async_read_some(asio::buffer(state->buffer), [state, &test](const boost::system::error_code& read_error, std::size_t bytes) {
            if (test.finished()) {
                return;
            }
            if (read_error == asio::error::eof && bytes == 0) {
                test.pass();
                return;
            }
            test.fail("expected EOF after peer shutdown");
        });
    });
}

void test_tcp_pending_io_cancellation(AsyncTest& test)
{
    struct State : TcpState {
        explicit State(asio::io_context& context) : TcpState(context) {}
        std::array<char, 32> buffer{};
    };
    auto state = std::make_shared<State>(test.context());
    test.set_cleanup([state] {
        state->cancel_timer.cancel();
        close_acceptor(state->acceptor);
        close_socket(state->server);
        close_socket(state->client);
    });

    start_loopback_listener(state->acceptor);
    const auto endpoint = state->acceptor.local_endpoint();
    state->acceptor.async_accept(state->server, [state, &test](const boost::system::error_code& error) {
        if (!test.finished() && error) {
            test.fail("accept failed: " + error.message());
        }
    });
    state->client.async_connect(endpoint, [state, &test](const boost::system::error_code& error) {
        if (test.finished()) {
            return;
        }
        if (error) {
            test.fail("connect failed: " + error.message());
            return;
        }
        state->client.async_read_some(asio::buffer(state->buffer), [state, &test](const boost::system::error_code& read_error, std::size_t) {
            if (test.finished()) {
                return;
            }
            if (read_error == asio::error::operation_aborted) {
                test.pass();
                return;
            }
            test.fail("pending read was not cancelled by the timer");
        });
        state->cancel_timer.expires_after(std::chrono::milliseconds{50});
        state->cancel_timer.async_wait([state, &test](const boost::system::error_code& timer_error) {
            if (test.finished() || timer_error) {
                return;
            }
            boost::system::error_code cancel_error;
            state->client.cancel(cancel_error);
            if (cancel_error) {
                test.fail("socket cancellation failed: " + cancel_error.message());
            }
        });
    });
}

struct HttpState : TcpState {
    explicit HttpState(asio::io_context& context) : TcpState(context) {}
    beast::flat_buffer server_buffer;
    beast::flat_buffer client_buffer;
};

void test_http_get_response(AsyncTest& test)
{
    struct State : HttpState {
        explicit State(asio::io_context& context) : HttpState(context) {}
        http::request<http::string_body> server_request;
        http::request<http::string_body> client_request;
        http::response<http::string_body> server_response;
        http::response<http::string_body> client_response;
    };
    auto state = std::make_shared<State>(test.context());
    test.set_cleanup([state] {
        close_acceptor(state->acceptor);
        close_socket(state->server);
        close_socket(state->client);
    });

    start_loopback_listener(state->acceptor);
    const auto endpoint = state->acceptor.local_endpoint();
    state->acceptor.async_accept(state->server, [state, &test](const boost::system::error_code& error) {
        if (error) {
            test.fail("accept failed: " + error.message());
            return;
        }
        http::async_read(state->server, state->server_buffer, state->server_request, [state, &test](const boost::system::error_code& read_error, std::size_t) {
            if (read_error) {
                test.fail("server request read failed: " + read_error.message());
                return;
            }
            if (state->server_request.method() != http::verb::get || state->server_request.target() != "/compat") {
                test.fail("server received an unexpected GET request");
                return;
            }
            state->server_response = {http::status::ok, state->server_request.version()};
            state->server_response.set("X-Interlink-Test", "NET-HTTP-001");
            state->server_response.body() = "loopback-get";
            state->server_response.prepare_payload();
            state->server_response.keep_alive(false);
            http::async_write(state->server, state->server_response, [state, &test](const boost::system::error_code& write_error, std::size_t) {
                if (write_error) {
                    test.fail("server response write failed: " + write_error.message());
                }
            });
        });
    });
    state->client.async_connect(endpoint, [state, &test](const boost::system::error_code& error) {
        if (error) {
            test.fail("connect failed: " + error.message());
            return;
        }
        state->client_request = {http::verb::get, "/compat", 11};
        state->client_request.set(http::field::host, "127.0.0.1");
        http::async_write(state->client, state->client_request, [state, &test](const boost::system::error_code& write_error, std::size_t) {
            if (write_error) {
                test.fail("client request write failed: " + write_error.message());
                return;
            }
            http::async_read(state->client, state->client_buffer, state->client_response, [state, &test](const boost::system::error_code& read_error, std::size_t) {
                if (read_error) {
                    test.fail("client response read failed: " + read_error.message());
                    return;
                }
                if (state->client_response.result() == http::status::ok &&
                    state->client_response.body() == "loopback-get" &&
                    state->client_response["X-Interlink-Test"] == "NET-HTTP-001") {
                    test.pass();
                    return;
                }
                test.fail("GET response status, body, or header did not match");
            });
        });
    });
}

void test_http_keep_alive_boundaries(AsyncTest& test)
{
    struct State : HttpState {
        explicit State(asio::io_context& context) : HttpState(context) {}
        http::request<http::string_body> server_request;
        http::request<http::string_body> client_request;
        http::response<http::string_body> server_response;
        http::response<http::string_body> client_response;
        unsigned request_count{0};
    };
    auto state = std::make_shared<State>(test.context());
    test.set_cleanup([state] {
        close_acceptor(state->acceptor);
        close_socket(state->server);
        close_socket(state->client);
    });

    const auto serve_next = std::make_shared<std::function<void()>>();
    const std::weak_ptr<std::function<void()>> weak_serve_next{serve_next};
    *serve_next = [state, &test, weak_serve_next] {
        const auto next = weak_serve_next.lock();
        if (!next) {
            return;
        }
        state->server_request = {};
        http::async_read(state->server, state->server_buffer, state->server_request, [state, &test, next](const boost::system::error_code& read_error, std::size_t) {
            if (read_error) {
                test.fail("server request read failed: " + read_error.message());
                return;
            }
            ++state->request_count;
            const bool first = state->request_count == 1;
            const bool valid = (first && state->server_request.target() == "/present") ||
                               (!first && state->server_request.target() == "/missing");
            if (!valid || state->request_count > 2) {
                test.fail("unexpected keep-alive request sequence");
                return;
            }
            state->server_response = {
                first ? http::status::ok : http::status::not_found, state->server_request.version()};
            state->server_response.body() = first ? "present" : "missing";
            state->server_response.keep_alive(first);
            state->server_response.prepare_payload();
            http::async_write(state->server, state->server_response, [state, &test, next, first](const boost::system::error_code& write_error, std::size_t) {
                if (write_error) {
                    test.fail("server response write failed: " + write_error.message());
                    return;
                }
                if (first) {
                    (*next)();
                }
            });
        });
    };

    start_loopback_listener(state->acceptor);
    const auto endpoint = state->acceptor.local_endpoint();
    state->acceptor.async_accept(state->server, [state, &test, serve_next](const boost::system::error_code& error) {
        if (error) {
            test.fail("accept failed: " + error.message());
            return;
        }
        (*serve_next)();
    });
    const auto send_second = std::make_shared<std::function<void()>>();
    *send_second = [state, &test] {
        state->client_request = {http::verb::get, "/missing", 11};
        state->client_request.set(http::field::host, "127.0.0.1");
        state->client_request.keep_alive(false);
        http::async_write(state->client, state->client_request, [state, &test](const boost::system::error_code& write_error, std::size_t) {
            if (write_error) {
                test.fail("second request write failed: " + write_error.message());
                return;
            }
            state->client_response = {};
            http::async_read(state->client, state->client_buffer, state->client_response, [state, &test](const boost::system::error_code& read_error, std::size_t) {
                if (read_error) {
                    test.fail("second response read failed: " + read_error.message());
                    return;
                }
                if (state->client_response.result() == http::status::not_found && state->client_response.body() == "missing") {
                    test.pass();
                    return;
                }
                test.fail("second response boundary or content did not match");
            });
        });
    };
    state->client.async_connect(endpoint, [state, &test, send_second](const boost::system::error_code& error) {
        if (error) {
            test.fail("connect failed: " + error.message());
            return;
        }
        state->client_request = {http::verb::get, "/present", 11};
        state->client_request.set(http::field::host, "127.0.0.1");
        state->client_request.keep_alive(true);
        http::async_write(state->client, state->client_request, [state, &test, send_second](const boost::system::error_code& write_error, std::size_t) {
            if (write_error) {
                test.fail("first request write failed: " + write_error.message());
                return;
            }
            state->client_response = {};
            http::async_read(state->client, state->client_buffer, state->client_response, [state, &test, send_second](const boost::system::error_code& read_error, std::size_t) {
                if (read_error) {
                    test.fail("first response read failed: " + read_error.message());
                    return;
                }
                if (state->client_response.result() != http::status::ok || state->client_response.body() != "present") {
                    test.fail("first response boundary or content did not match");
                    return;
                }
                (*send_second)();
            });
        });
    });
}

void test_http_truncated_request(AsyncTest& test)
{
    struct State : HttpState {
        explicit State(asio::io_context& context) : HttpState(context) {}
        http::request_parser<http::string_body> parser;
        std::string partial_request{"POST /partial HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 5\r\n\r\nab"};
    };
    auto state = std::make_shared<State>(test.context());
    test.set_cleanup([state] {
        close_acceptor(state->acceptor);
        close_socket(state->server);
        close_socket(state->client);
    });

    start_loopback_listener(state->acceptor);
    const auto endpoint = state->acceptor.local_endpoint();
    state->acceptor.async_accept(state->server, [state, &test](const boost::system::error_code& error) {
        if (error) {
            test.fail("accept failed: " + error.message());
            return;
        }
        http::async_read(state->server, state->server_buffer, state->parser, [state, &test](const boost::system::error_code& read_error, std::size_t) {
            if (read_error == http::error::partial_message) {
                test.pass();
                return;
            }
            if (!read_error) {
                test.fail("truncated request was accepted");
                return;
            }
            test.fail("expected Beast partial_message, got: " + read_error.message());
        });
    });
    state->client.async_connect(endpoint, [state, &test](const boost::system::error_code& error) {
        if (error) {
            test.fail("connect failed: " + error.message());
            return;
        }
        asio::async_write(state->client, asio::buffer(state->partial_request), [state, &test](const boost::system::error_code& write_error, std::size_t) {
            if (write_error) {
                test.fail("truncated request write failed: " + write_error.message());
                return;
            }
            boost::system::error_code shutdown_error;
            state->client.shutdown(tcp::socket::shutdown_send, shutdown_error);
            if (shutdown_error) {
                test.fail("client shutdown failed: " + shutdown_error.message());
            }
        });
    });
}

bool run_case(const std::string& id)
{
    AsyncTest test;
    if (id == "NET-TCP-001") return test.run(test_tcp_connect_accept);
    if (id == "NET-TCP-002") return test.run(test_tcp_connection_refused);
    if (id == "NET-TCP-003") return test.run(test_tcp_eof_shutdown);
    if (id == "NET-TCP-004") return test.run(test_tcp_pending_io_cancellation);
    if (id == "NET-HTTP-001") return test.run(test_http_get_response);
    if (id == "NET-HTTP-002") return test.run(test_http_keep_alive_boundaries);
    if (id == "NET-HTTP-003") return test.run(test_http_truncated_request);
    std::cerr << "unknown network compatibility test: " << id << '\n';
    return false;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "usage: nmos-interlink-network-compatibility <test-id>\n";
        return 2;
    }
    return run_case(argv[1]) ? 0 : 1;
}
