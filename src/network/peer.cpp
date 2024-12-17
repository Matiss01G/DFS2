#include "../../include/network/peer.hpp"
#include "../../include/network/tcp_peer.hpp"
#include <boost/log/trivial.hpp>

namespace dfs::network {

std::shared_ptr<IPeer> create_tcp_peer(
    boost::asio::io_context& io_context,
    const std::string& address,
    uint16_t port) {
    
    BOOST_LOG_TRIVIAL(debug) << "Creating TCP peer for " << address << ":" << port;
    try {
        auto peer = std::make_shared<TcpPeer>(io_context, address, port);
        BOOST_LOG_TRIVIAL(info) << "Successfully created TCP peer";
        return peer;
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to create TCP peer: " << e.what();
        throw;
    }
}

} // namespace dfs::network
