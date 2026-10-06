#include "consensus/chainparams.hpp"
#include "crypto/sha256.hpp"
#include "net/address.hpp"
#include "net/discovery.hpp"
#include "net/peer.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

std::filesystem::path unique_dir(
    std::string_view suffix)
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    return std::filesystem::temp_directory_path() /
        ("quintum-stage16-" +
         std::string(suffix) + "-" +
         std::to_string(stamp));
}

quintum::net::VersionMessage version(
    std::uint64_t nonce,
    std::uint32_t height)
{
    return quintum::net::VersionMessage{
        .protocol_version =
            quintum::net::kProtocolVersion,
        .services = 1U,
        .timestamp = 1'790'970'000ULL,
        .nonce = nonce,
        .start_height = height,
    };
}

quintum::net::VersionMessage addrv2_version(
    std::uint64_t nonce,
    std::uint32_t height)
{
    auto value = version(nonce, height);
    value.services |=
        quintum::net::kServiceAddrV2;
    return value;
}

quintum::net::PeerAddress local_address(
    std::uint16_t port,
    std::uint64_t seen = 1'790'970'000ULL)
{
    const auto ipv4 =
        quintum::net::parse_ipv4("127.0.0.1");
    assert(ipv4.has_value());

    return quintum::net::PeerAddress{
        .ipv4 = *ipv4,
        .port = port,
        .services = 1U,
        .last_seen = seen,
    };
}

quintum::net::PeerAddress tor_address(
    std::uint16_t port,
    std::uint64_t seen = 1'790'970'000ULL)
{
    return quintum::net::PeerAddress{
        .network =
            quintum::net::AddressNetwork::tor_v3,
        .host =
            std::string(56U, 'a') + ".onion",
        .port = port,
        .services = 1U,
        .last_seen = seen,
    };
}

quintum::net::PeerAddress i2p_address(
    std::uint16_t port,
    std::uint64_t seen = 1'790'970'000ULL)
{
    return quintum::net::PeerAddress{
        .network =
            quintum::net::AddressNetwork::i2p,
        .host =
            std::string(52U, 'b') + ".b32.i2p",
        .port = port,
        .services = 1U,
        .last_seen = seen,
    };
}

void write_legacy_peer_store(
    const quintum::consensus::ChainParams& params,
    const std::filesystem::path& path,
    const quintum::net::PeerAddress& peer)
{
    using namespace quintum;

    Bytes body{
        'Q', 'P', 'E', 'E', 'R', 'S', '1', 0
    };

    append_little_endian(
        body,
        std::uint32_t{1U}
    );
    body.push_back(
        static_cast<Byte>(params.network)
    );
    body.insert(
        body.end(),
        params.message_start.begin(),
        params.message_start.end()
    );

    append_compact_size(body, 1U);
    append_little_endian(body, peer.ipv4);
    append_little_endian(body, peer.port);
    append_little_endian(body, peer.services);
    append_little_endian(body, peer.last_seen);
    append_little_endian(body, std::uint64_t{300U});
    append_little_endian(body, std::uint64_t{250U});
    append_little_endian(body, std::uint64_t{360U});
    append_little_endian(body, std::uint32_t{2U});

    const auto checksum =
        crypto::double_sha256(body);

    body.insert(
        body.end(),
        checksum.begin(),
        checksum.end()
    );

    std::filesystem::create_directories(
        path.parent_path()
    );

    std::ofstream out(
        path,
        std::ios::binary |
        std::ios::trunc
    );

    assert(out.good());
    out.write(
        reinterpret_cast<const char*>(
            body.data()),
        static_cast<std::streamsize>(
            body.size())
    );
    out.close();
    assert(out.good());
}

void test_addrv2_codec()
{
    using namespace quintum::net;

    const auto ipv4 =
        parse_ipv4("203.0.113.7");
    assert(ipv4.has_value());

    const PeerAddress public_peer{
        .ipv4 = *ipv4,
        .port = 39444U,
        .services = 1U,
        .last_seen = 100U,
    };

    const auto tor =
        tor_address(39444U, 101U);
    const auto i2p =
        i2p_address(39444U, 102U);

    assert(valid_peer_address(tor, false));
    assert(valid_peer_address(i2p, false));
    assert(is_overlay_address(tor));
    assert(is_overlay_address(i2p));
    assert(format_peer_host(tor) == tor.host);
    assert(format_peer_host(i2p) == i2p.host);

    const std::array<PeerAddress, 3> values{
        public_peer,
        tor,
        i2p
    };

    const auto payload =
        serialize_addresses_v2(values);
    const auto decoded =
        parse_addresses_v2(
            payload,
            false
        );

    assert(decoded.has_value());
    assert(decoded->size() == 3U);
    assert((*decoded)[0] == public_peer);
    assert((*decoded)[1] == tor);
    assert((*decoded)[2] == i2p);

    const auto legacy =
        serialize_addresses(values);
    const auto legacy_decoded =
        parse_addresses(
            legacy,
            false
        );

    assert(legacy_decoded.has_value());
    assert(legacy_decoded->size() == 1U);
    assert(legacy_decoded->front() ==
           public_peer);

    auto malformed = payload;
    assert(malformed.size() > 2U);
    malformed[1] = 0xffU;
    assert(!parse_addresses_v2(
        malformed,
        false
    ));
}

void test_addrman_v1_to_v2_migration()
{
    using namespace quintum::net;

    const auto dir =
        unique_dir("v1-migration");
    const auto& params =
        quintum::consensus::regtest_params();

    const auto ipv4 =
        parse_ipv4("127.0.0.1");
    assert(ipv4.has_value());

    const PeerAddress legacy{
        .ipv4 = *ipv4,
        .port = 49130U,
        .services = 7U,
        .last_seen = 200U,
    };

    AddrManager manager{
        params,
        dir,
        true
    };

    write_legacy_peer_store(
        params,
        manager.path(),
        legacy
    );

    assert(manager.load() ==
           AddrStoreError::none);
    assert(manager.size() == 1U);
    assert(manager.entries()[0].address ==
           legacy);
    assert(manager.entries()[0].last_attempt ==
           300U);
    assert(manager.entries()[0].last_success ==
           250U);
    assert(manager.entries()[0].next_attempt ==
           360U);
    assert(manager.entries()[0].failures ==
           2U);

    assert(manager.save() ==
           AddrStoreError::none);

    std::ifstream migrated(
        manager.path(),
        std::ios::binary
    );
    assert(migrated.good());

    std::array<unsigned char, 12> prefix{};
    migrated.read(
        reinterpret_cast<char*>(
            prefix.data()),
        static_cast<std::streamsize>(
            prefix.size())
    );
    assert(migrated.good());

    const std::uint32_t version =
        static_cast<std::uint32_t>(
            prefix[8]) |
        (static_cast<std::uint32_t>(
             prefix[9]) << 8U) |
        (static_cast<std::uint32_t>(
             prefix[10]) << 16U) |
        (static_cast<std::uint32_t>(
             prefix[11]) << 24U);

    assert(version == 2U);

    AddrManager reloaded{
        params,
        dir,
        true
    };

    assert(reloaded.load() ==
           AddrStoreError::none);
    assert(reloaded.size() == 1U);
    assert(reloaded.entries()[0].address ==
           legacy);
    assert(reloaded.entries()[0].failures ==
           2U);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void test_addrman_overlay_persistence()
{
    using namespace quintum::net;

    const auto dir =
        unique_dir("overlay-persist");
    const auto& params =
        quintum::consensus::regtest_params();

    AddrManager manager{
        params,
        dir,
        true
    };

    const auto tor =
        tor_address(39444U, 500U);
    const auto i2p =
        i2p_address(39444U, 501U);

    assert(manager.add(tor));
    assert(manager.add(i2p));
    manager.mark_failure(tor, 600U);
    manager.mark_success(i2p, 601U);

    assert(manager.save() ==
           AddrStoreError::none);

    AddrManager reloaded{
        params,
        dir,
        true
    };

    assert(reloaded.load() ==
           AddrStoreError::none);
    assert(reloaded.size() == 2U);

    bool saw_tor{false};
    bool saw_i2p{false};

    for (const auto& info :
         reloaded.entries()) {
        if (info.address == tor) {
            saw_tor = true;
            assert(info.failures == 1U);
        }

        if (info.address == i2p) {
            saw_i2p = true;
            assert(info.failures == 0U);
            assert(info.last_success == 601U);
        }
    }

    assert(saw_tor);
    assert(saw_i2p);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void test_address_codec()
{
    using namespace quintum::net;

    const auto parsed =
        parse_ipv4("203.0.113.7");
    assert(parsed.has_value());
    assert(format_ipv4(*parsed) ==
           "203.0.113.7");

    assert(!parse_ipv4("203.0.113").has_value());
    assert(!parse_ipv4("300.1.1.1").has_value());

    const PeerAddress public_peer{
        .ipv4 = *parsed,
        .port = 28444U,
        .services = 1U,
        .last_seen = 100U,
    };

    const auto loopback =
        local_address(48444U);

    assert(valid_peer_address(
        public_peer,
        false
    ));
    assert(!valid_peer_address(
        loopback,
        false
    ));
    assert(valid_peer_address(
        loopback,
        true
    ));

    const std::array<PeerAddress, 2> addresses{
        public_peer,
        public_peer
    };

    const auto payload =
        serialize_addresses(addresses);

    const auto decoded =
        parse_addresses(payload, false);

    assert(decoded.has_value());
    assert(decoded->size() == 1U);
    assert(decoded->front() == public_peer);
}

void test_addrman_persistence_and_network_binding()
{
    using namespace quintum::net;

    const auto dir = unique_dir("persist");
    const auto& regtest =
        quintum::consensus::regtest_params();

    AddrManager manager{
        regtest,
        dir,
        true
    };

    assert(manager.load() ==
           AddrStoreError::not_found);

    const auto peer =
        local_address(49123U, 200U);

    assert(manager.add(peer));
    manager.mark_attempt(peer, 300U);
    manager.mark_failure(peer, 300U);

    assert(manager.save() ==
           AddrStoreError::none);

    AddrManager reloaded{
        regtest,
        dir,
        true
    };

    assert(reloaded.load() ==
           AddrStoreError::none);
    assert(reloaded.size() == 1U);
    assert(reloaded.entries()[0].failures == 1U);
    assert(reloaded.entries()[0].last_attempt == 300U);
    assert(reloaded.entries()[0].next_attempt == 360U);

    AddrManager wrong_network{
        quintum::consensus::testnet_params(),
        dir,
        true
    };

    assert(wrong_network.load() ==
           AddrStoreError::wrong_network);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void test_addrman_corruption_detection()
{
    using namespace quintum::net;

    const auto dir = unique_dir("corrupt");
    AddrManager manager{
        quintum::consensus::regtest_params(),
        dir,
        true
    };

    assert(manager.add(
        local_address(49124U)));
    assert(manager.save() ==
           AddrStoreError::none);

    std::fstream file(
        manager.path(),
        std::ios::in |
        std::ios::out |
        std::ios::binary
    );
    assert(file.good());

    file.seekg(10, std::ios::beg);
    char value{0};
    file.read(&value, 1);
    assert(file.good());

    value = static_cast<char>(
        static_cast<unsigned char>(value) ^
        0x01U
    );

    file.seekp(10, std::ios::beg);
    file.write(&value, 1);
    file.flush();
    file.close();

    AddrManager corrupted{
        quintum::consensus::regtest_params(),
        dir,
        true
    };

    assert(corrupted.load() ==
           AddrStoreError::corrupt);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void test_network_hardcoded_seeds()
{
    using namespace quintum::net;

    const auto mainnet =
        hardcoded_seeds(
            quintum::consensus::Network::mainnet);
    const auto testnet =
        hardcoded_seeds(
            quintum::consensus::Network::testnet);
    const auto regtest =
        hardcoded_seeds(
            quintum::consensus::Network::regtest);
    const auto randomx =
        hardcoded_seeds(
            quintum::consensus::Network::randomx_testnet);

    assert(mainnet.empty());
    assert(regtest.empty());
    assert(testnet.size() == 1U);
    assert(testnet.front().host ==
           "212.193.15.139");
    assert(testnet.front().port == 38444U);
    assert(randomx.size() == 1U);
    assert(randomx.front().host ==
           "212.193.15.139");
    assert(randomx.front().port == 39444U);
}

void test_dns_seed_resolution()
{
    using namespace quintum::net;

    const auto dir = unique_dir("dns-seed");
    const auto& params =
        quintum::consensus::regtest_params();

    AddrManager manager{
        params,
        dir,
        true
    };
    PeerDiscovery discovery{manager};

    const std::array<SeedEndpoint, 1> seeds{
        SeedEndpoint{"localhost", 49003U},
    };

    const auto added =
        discovery.bootstrap_dns_seeds(
            seeds,
            5'000U
        );

    assert(added >= 1U);

    const auto addresses =
        manager.addresses();

    assert(!addresses.empty());

    for (const auto& address : addresses) {
        assert(address.port == 49003U);
        assert(address.services == 1U);
        assert(address.last_seen == 5'000U);
        assert(valid_peer_address(
            address,
            true
        ));
    }

    assert(manager.save() ==
           AddrStoreError::none);

    AddrManager reloaded{
        params,
        dir,
        true
    };

    assert(reloaded.load() ==
           AddrStoreError::none);
    assert(reloaded.size() ==
           manager.size());

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void test_retry_backoff_and_seed_bootstrap()
{
    using namespace quintum::net;

    const auto dir = unique_dir("backoff");
    AddrManager manager{
        quintum::consensus::regtest_params(),
        dir,
        true
    };
    PeerDiscovery discovery{manager};

    const std::array<SeedEndpoint, 2> seeds{
        SeedEndpoint{"127.0.0.1", 49001U},
        SeedEndpoint{"127.0.0.1", 49002U},
    };

    assert(discovery.bootstrap_seeds(
               seeds,
               1'000U) == 2U);
    assert(discovery.bootstrap_seeds(
               seeds,
               1'001U) == 0U);

    const auto first =
        manager.select(1'000U);
    assert(first.has_value());

    manager.mark_failure(*first, 1'000U);
    const auto& failed = manager.entries()[0].address == *first
        ? manager.entries()[0]
        : manager.entries()[1];

    assert(failed.failures == 1U);
    assert(failed.next_attempt == 1'060U);

    const std::array<PeerAddress, 1> excluded{
        *first
    };

    const auto selected =
        manager.select(
            1'000U,
            excluded
        );

    assert(selected.has_value());
    assert(selected->port != first->port);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void test_addrman_public_subnet_diversity()
{
    using namespace quintum::net;

    const auto dir =
        unique_dir("subnet-diversity");
    AddrManager manager{
        quintum::consensus::testnet_params(),
        dir,
        false
    };

    auto make_public =
        [](std::uint32_t third,
           std::uint16_t port) {
            const auto ip =
                parse_ipv4(
                    "203.0." +
                    std::to_string(third) +
                    ".1");
            assert(ip.has_value());

            return PeerAddress{
                .ipv4 = *ip,
                .port = port,
                .services = 1U,
                .last_seen = 5'000U,
            };
        };

    for (std::size_t i = 0U;
         i < kMaxAddrEntriesPerIpv4Group;
         ++i) {
        assert(manager.add(
            make_public(
                static_cast<std::uint32_t>(i),
                static_cast<std::uint16_t>(
                    40'000U + i)
            )
        ));
    }

    assert(!manager.add(
        make_public(200U, 49'999U)
    ));

    const auto other_ip =
        parse_ipv4("198.51.100.7");
    assert(other_ip.has_value());

    const PeerAddress other{
        .ipv4 = *other_ip,
        .port = 45'000U,
        .services = 1U,
        .last_seen = 5'001U,
    };
    assert(manager.add(other));

    const std::array<PeerAddress, 1>
        excluded{
            manager.entries().front().address
        };

    const auto selected =
        manager.select(
            5'001U,
            excluded
        );

    assert(selected.has_value());
    assert((selected->ipv4 >> 16U) !=
           (excluded.front().ipv4 >> 16U));

    const auto advertised =
        manager.addresses(2U);
    assert(advertised.size() == 2U);
    assert((advertised[0].ipv4 >> 16U) !=
           (advertised[1].ipv4 >> 16U));

    std::error_code ec;
    std::filesystem::remove_all(
        dir,
        ec
    );
}

void test_real_addrv2_exchange()
{
    using namespace quintum::net;

    const auto& params =
        quintum::consensus::regtest_params();

    PeerListener listener{params};

    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    const std::array<PeerAddress, 2>
        advertised{
            tor_address(39444U, 7'000U),
            i2p_address(39444U, 7'001U),
        };

    PeerError server_error{
        PeerError::accept_failed
    };

    std::thread server([&] {
        auto accepted =
            listener.accept_and_handshake(
                addrv2_version(
                    0xa201U,
                    10U),
                5'000U
            );

        server_error = accepted.error;

        if (!accepted.ok()) {
            return;
        }

        server_error =
            accepted.session->
                service_discovery_once(
                    advertised,
                    true
                );

        accepted.session->close();
    });

    auto client =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            addrv2_version(
                0xa202U,
                9U),
            5'000U
        );

    assert(client.ok());

    std::vector<PeerAddress> learned;
    assert(client.session->request_addresses(
               true,
               learned) ==
           PeerError::none);

    assert(learned.size() == 2U);
    assert(learned[0] == advertised[0]);
    assert(learned[1] == advertised[1]);

    client.session->close();
    server.join();

    assert(server_error ==
           PeerError::none);
}

void test_real_peer_discovery_chain()
{
    using namespace quintum::net;

    const auto& params =
        quintum::consensus::regtest_params();
    const auto dir_a = unique_dir("node-a");
    const auto dir_b = unique_dir("node-b");

    PeerListener listener_b{params};
    PeerListener listener_c{params};

    assert(listener_b.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);
    assert(listener_c.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    AddrManager manager_b{
        params,
        dir_b,
        true
    };
    assert(manager_b.add(
        local_address(
            listener_c.local_port(),
            2'000U
        )));

    const auto advertised_by_b =
        manager_b.addresses();

    PeerError b_error{
        PeerError::accept_failed
    };

    std::thread node_b([&] {
        auto accepted =
            listener_b.accept_and_handshake(
                version(0xb001U, 11U),
                5'000U
            );

        if (!accepted.ok()) {
            b_error = accepted.error;
            return;
        }

        b_error =
            accepted.session->service_discovery_once(
                advertised_by_b,
                true
            );
        accepted.session->close();
    });

    AddrManager manager_a{
        params,
        dir_a,
        true
    };

    const auto address_b =
        local_address(
            listener_b.local_port(),
            2'000U
        );
    assert(manager_a.add(address_b));

    PeerDiscovery discovery_a{manager_a};

    auto to_b =
        discovery_a.connect_one(
            params,
            version(0xa001U, 5U),
            3'000U,
            5'000U
        );

    assert(to_b.ok());
    assert(to_b.address.has_value());
    assert(to_b.address->port ==
           listener_b.local_port());

    const auto learned =
        discovery_a.learn_from_peer(
            *to_b.session,
            true
        );

    assert(learned.ok());
    assert(learned.received == 1U);
    assert(learned.added == 1U);
    assert(manager_a.size() == 2U);

    to_b.session->close();
    node_b.join();
    assert(b_error == PeerError::none);

    PeerError c_error{
        PeerError::accept_failed
    };

    std::thread node_c([&] {
        auto accepted =
            listener_c.accept_and_handshake(
                version(0xc001U, 12U),
                5'000U
            );

        c_error = accepted.error;

        if (accepted.ok()) {
            accepted.session->close();
        }
    });

    auto to_c =
        discovery_a.connect_one(
            params,
            version(0xa002U, 5U),
            3'000U,
            5'000U
        );

    assert(to_c.ok());
    assert(to_c.address.has_value());
    assert(to_c.address->port ==
           listener_c.local_port());

    to_c.session->close();
    node_c.join();
    assert(c_error == PeerError::none);

    AddrManager restarted_a{
        params,
        dir_a,
        true
    };

    assert(restarted_a.load() ==
           AddrStoreError::none);
    assert(restarted_a.size() == 2U);

    const auto known =
        restarted_a.addresses();
    bool found_b{false};
    bool found_c{false};

    for (const auto& item : known) {
        found_b = found_b ||
            item.port == listener_b.local_port();
        found_c = found_c ||
            item.port == listener_c.local_port();
    }

    assert(found_b);
    assert(found_c);

    std::error_code ec;
    std::filesystem::remove_all(dir_a, ec);
    std::filesystem::remove_all(dir_b, ec);
}

void test_initialize_reenables_persisted_hardcoded_seed()
{
    using namespace quintum::net;

    const auto dir =
        unique_dir("persisted-seed-retry");
    const auto& params =
        quintum::consensus::randomx_testnet_params();

    const auto seed_ip =
        parse_ipv4("212.193.15.139");
    assert(seed_ip.has_value());

    const PeerAddress seed{
        .ipv4 = *seed_ip,
        .port = 39444U,
        .services = 1U,
        .last_seen = 1'000U,
    };

    {
        AddrManager manager{
            params,
            dir,
            false
        };

        assert(manager.add(seed));

        manager.mark_failure(
            seed,
            1'000U
        );

        assert(manager.entries().size() == 1U);
        assert(manager.entries().front().failures == 1U);
        assert(manager.entries().front().next_attempt == 1'060U);
        assert(manager.save() == AddrStoreError::none);
    }

    AddrManager restarted{
        params,
        dir,
        false
    };
    PeerDiscovery discovery{restarted};

    // Before this regression fix, initialize() returned immediately for a
    // non-empty peers.dat, leaving the fixed seed blocked until 1060.
    assert(discovery.initialize(
               quintum::consensus::Network::randomx_testnet,
               1'001U) ==
           AddrStoreError::none);

    assert(restarted.entries().size() == 1U);
    assert(restarted.entries().front().failures == 1U);
    assert(restarted.entries().front().next_attempt <= 1'001U);

    const auto selected =
        restarted.select(1'001U);

    assert(selected.has_value());
    assert(selected->ipv4 == *seed_ip);
    assert(selected->port == 39444U);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void test_connect_any_skips_failed_peer()
{
    using namespace quintum::net;

    const auto& params =
        quintum::consensus::regtest_params();
    const auto dir = unique_dir("fallback");

    PeerListener temporary_bad{params};
    assert(temporary_bad.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    const auto bad_port =
        temporary_bad.local_port();

    PeerListener good_listener{params};
    assert(good_listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);
    assert(good_listener.local_port() !=
           bad_port);

    temporary_bad.close();

    AddrManager manager{
        params,
        dir,
        true
    };

    assert(manager.add(
        local_address(bad_port, 4'000U)));
    assert(manager.add(
        local_address(
            good_listener.local_port(),
            4'000U
        )));

    PeerError good_error{
        PeerError::accept_failed
    };

    std::thread good_peer([&] {
        auto accepted =
            good_listener.accept_and_handshake(
                version(0xd001U, 30U),
                5'000U
            );

        good_error = accepted.error;
        if (accepted.ok()) {
            accepted.session->close();
        }
    });

    PeerDiscovery discovery{manager};
    auto connected =
        discovery.connect_any(
            params,
            version(0xe001U, 20U),
            5'000U,
            500U,
            2U
        );

    assert(connected.ok());
    assert(connected.address.has_value());
    assert(connected.address->port ==
           good_listener.local_port());

    connected.session->close();
    good_peer.join();
    assert(good_error == PeerError::none);

    const auto& entries = manager.entries();
    assert(entries.size() == 2U);

    const auto failed_it =
        entries[0].address.port == bad_port
            ? &entries[0]
            : &entries[1];

    assert(failed_it->failures == 1U);
    assert(failed_it->next_attempt > 5'000U);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

} // namespace

int main()
{
    test_addrv2_codec();
    test_addrman_v1_to_v2_migration();
    test_addrman_overlay_persistence();
    test_address_codec();
    test_addrman_persistence_and_network_binding();
    test_addrman_corruption_detection();
    test_network_hardcoded_seeds();
    test_dns_seed_resolution();
    test_retry_backoff_and_seed_bootstrap();
    test_addrman_public_subnet_diversity();
    test_real_addrv2_exchange();
    test_real_peer_discovery_chain();
    test_initialize_reenables_persisted_hardcoded_seed();
    test_connect_any_skips_failed_peer();
    return 0;
}
