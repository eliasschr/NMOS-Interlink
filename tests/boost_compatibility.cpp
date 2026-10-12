#include <boost/asio.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/json.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

int main()
{
    const boost::json::object original{{"component", "nmos-interlink"}, {"phase", "1B.1"}};
    const auto parsed = boost::json::parse(boost::json::serialize(original)).as_object();
    if (parsed != original) {
        return 1;
    }

    const auto uuid = boost::uuids::random_generator{}();
    const auto parsed_uuid = boost::uuids::string_generator{}(boost::uuids::to_string(uuid));
    if (parsed_uuid != uuid) {
        return 2;
    }

    boost::beast::flat_buffer buffer;
    boost::asio::ssl::context ssl_context{boost::asio::ssl::context::tls_client};
    (void)buffer;
    (void)ssl_context;
    return 0;
}
