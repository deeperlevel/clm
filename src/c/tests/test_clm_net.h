#pragma once

#include "test.h"
#include "../clm/net.h"

static int clm_net_test_send_all(clm_id socket_id, const void* bytes,
    size_t length)
{
    if (length == 0) {
        return 1;
    }
    const unsigned char* data = (const unsigned char*)bytes;
    size_t sent_total = 0;
    for (unsigned int attempt = 0; sent_total < length && attempt < 10;
         ++attempt) {
        size_t sent = 0;
        clm_net_status status = clm_net_send(socket_id, data + sent_total,
            length - sent_total, &sent);
        if (status == CLM_NET_WOULD_BLOCK) {
            unsigned int events = 0;
            if (clm_net_wait(socket_id, CLM_NET_EVENT_WRITE, 1000, &events) !=
                    CLM_NET_SUCCESS ||
                (events & CLM_NET_EVENT_WRITE) == 0) {
                return 0;
            }
        } else if (status != CLM_NET_SUCCESS || sent == 0) {
            return 0;
        } else {
            sent_total += sent;
        }
    }
    return sent_total == length;
}

static int clm_net_test_receive_exact(clm_id socket_id, void* buffer,
    size_t length)
{
    unsigned char* data = (unsigned char*)buffer;
    size_t received_total = 0;
    for (unsigned int attempt = 0; received_total < length && attempt < 10;
         ++attempt) {
        unsigned int events = 0;
        if (clm_net_wait(socket_id, CLM_NET_EVENT_READ, 1000, &events) !=
                CLM_NET_SUCCESS ||
            (events & CLM_NET_EVENT_READ) == 0) {
            return 0;
        }
        size_t received = 0;
        if (clm_net_receive(socket_id, data + received_total,
                length - received_total, &received) != CLM_NET_SUCCESS ||
            received == 0) {
            return 0;
        }
        received_total += received;
    }
    return received_total == length;
}

static int clm_net_test_string_equals(clm_id string_id,
    const char* expected)
{
    const char* text = NULL;
    size_t byte_length = 0;
    clm_int character_length = clm_string_length(string_id);
    return expected != NULL && character_length >= 0 &&
        clm_string_slice_borrow(string_id, 0, character_length, &text,
            &byte_length) == CLM_BPA_SUCCESS &&
        byte_length == strlen(expected) &&
        memcmp(text, expected, byte_length) == 0;
}

static int clm_net_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Net");

    clm_net_address ipv4 = {0};
    clm_net_address unchanged = {CLM_NET_IPV6, 123, {1}, 9};
    CLM_ASSERT(clm_net_socket_create(86101, CLM_NET_IPV4, CLM_NET_TCP) ==
            CLM_NET_ERROR_NOT_INITIALIZED &&
        clm_net_address_parse("127.0.0.1", 8080, &ipv4) == CLM_NET_SUCCESS &&
        ipv4.family == CLM_NET_IPV4 && ipv4.port == 8080 &&
        clm_net_address_parse("invalid", 1, &unchanged) ==
            CLM_NET_ERROR_INVALID_ARGUMENT &&
        unchanged.family == CLM_NET_IPV6 && unchanged.port == 123 &&
        unchanged.ip[0] == 1 && unchanged.scope_id == 9,
        "require initialization and parse numeric IPv4 atomically");

    clm_net_address ipv6 = {0};
    char text[64] = {0};
    char too_small[2] = {'x', 'y'};
    CLM_ASSERT(clm_net_address_parse("::1", 53, &ipv6) == CLM_NET_SUCCESS &&
        ipv6.family == CLM_NET_IPV6 && ipv6.port == 53 &&
        clm_net_address_format(&ipv6, text, sizeof(text)) == CLM_NET_SUCCESS &&
        strcmp(text, "::1") == 0 &&
        clm_net_address_format(&ipv6, too_small, sizeof(too_small)) ==
            CLM_NET_ERROR_BUFFER_TOO_SMALL &&
        too_small[0] == 'x' && too_small[1] == 'y',
        "parse and format IPv6 while preserving too-small output buffers");

    CLM_ASSERT(clm_net_initialize() == CLM_NET_SUCCESS &&
        clm_net_initialize() == CLM_NET_SUCCESS,
        "initialize networking idempotently");

    size_t resolved_count = 0;
    CLM_ASSERT(clm_net_resolve("127.0.0.1", 0, CLM_NET_TCP, NULL, 0,
            &resolved_count) == CLM_NET_ERROR_BUFFER_TOO_SMALL &&
        resolved_count == 1,
        "report required resolver capacity when the result array is empty");
    clm_net_address resolved[1] = {{0}};
    resolved_count = 0;
    CLM_ASSERT(clm_net_resolve("127.0.0.1", 0, CLM_NET_TCP, resolved, 1,
            &resolved_count) == CLM_NET_SUCCESS &&
        resolved_count == 1 && resolved[0].family == CLM_NET_IPV4 &&
        resolved[0].port == 0,
        "resolve numeric hosts to binary addresses with a host-order port");

    const clm_id host_string_id = 86201;
    const clm_id formatted_string_id = 86202;
    const clm_id payload_string_id = 86203;
    const char* string_payload = "CLM net \xe2\x9c\x93";
    int host_string_created = clm_string_new(host_string_id, "127.0.0.1");
    int formatted_string_created =
        clm_string_new(formatted_string_id, "old value");
    int payload_string_created =
        clm_string_new(payload_string_id, string_payload);
    int string_fixtures = host_string_created && formatted_string_created &&
        payload_string_created;
    clm_net_address string_address = {0};
    resolved_count = 0;
    CLM_ASSERT(string_fixtures &&
        clm_net_address_parse_string(host_string_id, 8123,
            &string_address) == CLM_NET_SUCCESS &&
        string_address.family == CLM_NET_IPV4 && string_address.port == 8123 &&
        clm_net_address_format_string(&string_address,
            formatted_string_id) == CLM_NET_SUCCESS &&
        clm_net_test_string_equals(formatted_string_id, "127.0.0.1") &&
        clm_net_resolve_string(host_string_id, 0, CLM_NET_UDP, resolved, 1,
            &resolved_count) == CLM_NET_SUCCESS &&
        resolved_count == 1 && resolved[0].family == CLM_NET_IPV4,
        "parse, format, and resolve addresses using CLM string IDs");

    const clm_id listener_id = 86110;
    const clm_id client_id = 86111;
    const clm_id accepted_id = 86112;
    int listener_created = clm_net_socket_create(listener_id, CLM_NET_IPV4,
        CLM_NET_TCP) == CLM_NET_SUCCESS;
    CLM_ASSERT(listener_created &&
        clm_net_socket_create(listener_id, CLM_NET_IPV4, CLM_NET_TCP) ==
            CLM_NET_ERROR_ALREADY_EXISTS,
        "reject a duplicate live socket ID without replacing its socket");
    clm_net_address loopback = {0};
    clm_net_address listener_address = {0};
    int listener_ready = listener_created &&
        clm_net_socket_set_reuse_address(listener_id, true) ==
            CLM_NET_SUCCESS &&
        clm_net_socket_set_nonblocking(listener_id, true) ==
            CLM_NET_SUCCESS &&
        clm_net_address_parse("127.0.0.1", 0, &loopback) ==
            CLM_NET_SUCCESS &&
        clm_net_bind(listener_id, &loopback) == CLM_NET_SUCCESS &&
        clm_net_listen(listener_id, 8) == CLM_NET_SUCCESS &&
        clm_net_socket_local_address(listener_id, &listener_address) ==
            CLM_NET_SUCCESS &&
        listener_address.port != 0;
    CLM_ASSERT(listener_ready,
        "bind an ephemeral loopback TCP listener and inspect its endpoint");
    CLM_ASSERT(listener_created &&
        clm_net_finalize() == CLM_NET_ERROR_BUSY,
        "refuse finalization while a socket remains registered");

    unsigned int observed = 99;
    CLM_ASSERT(listener_ready &&
        clm_net_wait(listener_id, CLM_NET_EVENT_READ, 0, &observed) ==
            CLM_NET_TIMEOUT &&
        observed == 0,
        "report a readiness timeout and clear the observed event flags");
    observed = 77;
    CLM_ASSERT(clm_net_wait(listener_id, 0, 0, &observed) ==
            CLM_NET_ERROR_INVALID_ARGUMENT && observed == 77,
        "reject invalid poll interests without changing output");
    CLM_ASSERT(listener_ready &&
        clm_net_accept(listener_id, accepted_id, NULL) == CLM_NET_WOULD_BLOCK,
        "return WOULD_BLOCK when a nonblocking listener has no pending client");

    int client_created = clm_net_socket_create(client_id, CLM_NET_IPV4,
        CLM_NET_TCP) == CLM_NET_SUCCESS;
    int connected = 0;
    if (client_created && listener_ready &&
        clm_net_socket_set_nonblocking(client_id, true) == CLM_NET_SUCCESS &&
        clm_net_socket_set_no_delay(client_id, true) == CLM_NET_SUCCESS) {
        clm_net_status connect_status = clm_net_connect(client_id,
            &listener_address);
        if (connect_status == CLM_NET_SUCCESS) {
            connected = 1;
        } else if (connect_status == CLM_NET_IN_PROGRESS) {
            for (unsigned int attempt = 0; attempt < 5; ++attempt) {
                unsigned int events = 0;
                if (clm_net_wait(client_id, CLM_NET_EVENT_WRITE, 1000,
                        &events) != CLM_NET_SUCCESS) {
                    break;
                }
                connect_status = clm_net_connect_finish(client_id);
                if (connect_status == CLM_NET_SUCCESS) {
                    connected = 1;
                    break;
                }
                if (connect_status != CLM_NET_IN_PROGRESS) {
                    break;
                }
            }
        }
    }
    CLM_ASSERT(connected,
        "complete a nonblocking TCP connection using readiness and SO_ERROR");

    clm_net_address accepted_peer = {0};
    int accepted = connected &&
        clm_net_accept(listener_id, accepted_id, &accepted_peer) ==
            CLM_NET_SUCCESS;
    CLM_ASSERT(accepted && accepted_peer.family == CLM_NET_IPV4 &&
        accepted_peer.port != 0,
        "accept and report the connecting peer's endpoint");

    clm_net_address client_peer = {0};
    CLM_ASSERT(connected &&
        clm_net_socket_peer_address(client_id, &client_peer) ==
            CLM_NET_SUCCESS &&
        client_peer.family == CLM_NET_IPV4 &&
        client_peer.port == listener_address.port,
        "inspect the remote endpoint of a connected TCP socket");

    const char request[] = "clm-net-request";
    clm_net_poll_entry poll_entries[1] = {{accepted_id,
        CLM_NET_EVENT_READ, 0}};
    size_t ready_count = 0;
    int request_sent = connected && accepted &&
        clm_net_test_send_all(client_id, request, sizeof(request) - 1);
    clm_net_status poll_status = request_sent ?
        clm_net_poll(poll_entries, 1, 1000, &ready_count) :
        CLM_NET_ERROR_SYSTEM;
    char received_request[sizeof(request)] = {0};
    int request_received = poll_status == CLM_NET_SUCCESS &&
        ready_count == 1 &&
        (poll_entries[0].revents & CLM_NET_EVENT_READ) != 0 &&
        clm_net_test_receive_exact(accepted_id, received_request,
            sizeof(request) - 1) &&
        memcmp(received_request, request, sizeof(request) - 1) == 0;
    CLM_ASSERT(request_received,
        "send a TCP byte stream and poll/receive it without message assumptions");

    const char response[] = "clm-net-response";
    int response_sent = accepted &&
        clm_net_test_send_all(accepted_id, response, sizeof(response) - 1);
    char received_response[sizeof(response)] = {0};
    int response_received = response_sent &&
        clm_net_test_receive_exact(client_id, received_response,
            sizeof(response) - 1) &&
        memcmp(received_response, response, sizeof(response) - 1) == 0;
    CLM_ASSERT(response_received,
        "exchange bytes in the reverse direction on the same TCP connection");

    const char* borrowed_payload = NULL;
    size_t borrowed_payload_length = 0;
    clm_int payload_characters = clm_string_length(payload_string_id);
    int payload_view_valid = payload_characters >= 0 &&
        clm_string_slice_borrow(payload_string_id, 0, payload_characters,
            &borrowed_payload, &borrowed_payload_length) == CLM_BPA_SUCCESS;
    size_t string_sent = 0;
    int string_sent_ok = payload_view_valid &&
        clm_net_send_string(client_id, payload_string_id, &string_sent) ==
            CLM_NET_SUCCESS &&
        string_sent == borrowed_payload_length;
    char received_text[64] = {0};
    int string_received_ok = string_sent_ok &&
        clm_net_test_receive_exact(accepted_id, received_text,
            borrowed_payload_length) &&
        memcmp(received_text, borrowed_payload, borrowed_payload_length) == 0;
    CLM_ASSERT(string_received_ok,
        "send a CLM string ID as UTF-8 bytes without a terminator");

    clm_net_status shutdown_status = accepted ?
        clm_net_socket_shutdown(accepted_id, CLM_NET_SHUTDOWN_WRITE) :
        CLM_NET_ERROR_NOT_FOUND;
    size_t closed_bytes = 99;
    char closed_buffer[1];
    int peer_closed = shutdown_status == CLM_NET_SUCCESS &&
        clm_net_wait(client_id, CLM_NET_EVENT_READ, 1000, &observed) ==
            CLM_NET_SUCCESS &&
        clm_net_receive(client_id, closed_buffer, sizeof(closed_buffer),
            &closed_bytes) == CLM_NET_CLOSED &&
        closed_bytes == 0;
    CLM_ASSERT(peer_closed,
        "report TCP orderly shutdown as CLOSED with a zero byte count");

    const clm_id udp_sender_id = 86120;
    const clm_id udp_receiver_id = 86121;
    int udp_sender_created = clm_net_socket_create(udp_sender_id, CLM_NET_IPV4,
        CLM_NET_UDP) == CLM_NET_SUCCESS;
    int udp_receiver_created = clm_net_socket_create(udp_receiver_id,
        CLM_NET_IPV4, CLM_NET_UDP) == CLM_NET_SUCCESS;
    clm_net_address udp_sender_address = {0};
    clm_net_address udp_receiver_address = {0};
    int udp_ready = udp_sender_created && udp_receiver_created &&
        clm_net_socket_set_nonblocking(udp_receiver_id, true) ==
            CLM_NET_SUCCESS &&
        clm_net_bind(udp_sender_id, &loopback) == CLM_NET_SUCCESS &&
        clm_net_bind(udp_receiver_id, &loopback) == CLM_NET_SUCCESS &&
        clm_net_socket_local_address(udp_sender_id, &udp_sender_address) ==
            CLM_NET_SUCCESS &&
        clm_net_socket_local_address(udp_receiver_id, &udp_receiver_address) ==
            CLM_NET_SUCCESS &&
        udp_sender_address.port != 0 && udp_receiver_address.port != 0;
    CLM_ASSERT(udp_ready,
        "bind UDP endpoints and query their assigned ephemeral ports");
    CLM_ASSERT(udp_receiver_created &&
        clm_net_listen(udp_receiver_id, 0) ==
            CLM_NET_ERROR_INVALID_ARGUMENT,
        "reject TCP listener operations on a UDP socket");

    size_t datagram_received = 777;
    clm_net_address datagram_source = {CLM_NET_IPV6, 123, {1}, 9};
    int no_datagram = udp_ready &&
        clm_net_receive_from(udp_receiver_id, text, sizeof(text),
            &datagram_received, &datagram_source) == CLM_NET_WOULD_BLOCK &&
        datagram_received == 777 && datagram_source.family == CLM_NET_IPV6 &&
        datagram_source.port == 123;
    CLM_ASSERT(no_datagram,
        "return WOULD_BLOCK without modifying datagram outputs when empty");

    size_t datagram_sent = 99;
    int empty_sent = udp_ready &&
        clm_net_send_to(udp_sender_id, &udp_receiver_address, NULL, 0,
            &datagram_sent) == CLM_NET_SUCCESS &&
        datagram_sent == 0;
    int empty_received = empty_sent &&
        clm_net_wait(udp_receiver_id, CLM_NET_EVENT_READ, 1000, &observed) ==
            CLM_NET_SUCCESS &&
        clm_net_receive_from(udp_receiver_id, text, sizeof(text),
            &datagram_received, &datagram_source) == CLM_NET_SUCCESS &&
        datagram_received == 0 &&
        datagram_source.port == udp_sender_address.port;
    CLM_ASSERT(empty_received,
        "transmit and receive a valid empty UDP datagram with its source");

    const char datagram[] = "clm-udp";
    int payload_sent = udp_ready &&
        clm_net_send_to(udp_sender_id, &udp_receiver_address, datagram,
            sizeof(datagram) - 1, &datagram_sent) == CLM_NET_SUCCESS &&
        datagram_sent == sizeof(datagram) - 1;
    int payload_received = payload_sent &&
        clm_net_wait(udp_receiver_id, CLM_NET_EVENT_READ, 1000, &observed) ==
            CLM_NET_SUCCESS &&
        clm_net_receive_from(udp_receiver_id, text, sizeof(text),
            &datagram_received, &datagram_source) == CLM_NET_SUCCESS &&
        datagram_received == sizeof(datagram) - 1 &&
        memcmp(text, datagram, sizeof(datagram) - 1) == 0;
    CLM_ASSERT(payload_received,
        "transmit one UDP datagram and report its payload length");

    datagram_sent = 99;
    int string_datagram_sent = udp_ready && payload_view_valid &&
        clm_net_send_to_string(udp_sender_id, &udp_receiver_address,
            payload_string_id, &datagram_sent) == CLM_NET_SUCCESS &&
        datagram_sent == borrowed_payload_length;
    int string_datagram_received = string_datagram_sent &&
        clm_net_wait(udp_receiver_id, CLM_NET_EVENT_READ, 1000, &observed) ==
            CLM_NET_SUCCESS &&
        clm_net_receive_from(udp_receiver_id, text, sizeof(text),
            &datagram_received, NULL) == CLM_NET_SUCCESS &&
        datagram_received == borrowed_payload_length &&
        memcmp(text, borrowed_payload, borrowed_payload_length) == 0;
    CLM_ASSERT(string_datagram_received,
        "send a CLM string ID as one UTF-8 UDP datagram");

    int cleanup_ok = 1;
    if (udp_receiver_created) {
        cleanup_ok &= clm_net_socket_close(udp_receiver_id) ==
            CLM_NET_SUCCESS;
    }
    if (udp_sender_created) {
        cleanup_ok &= clm_net_socket_close(udp_sender_id) ==
            CLM_NET_SUCCESS;
    }
    if (accepted) {
        cleanup_ok &= clm_net_socket_close(accepted_id) == CLM_NET_SUCCESS;
    }
    if (client_created) {
        cleanup_ok &= clm_net_socket_close(client_id) == CLM_NET_SUCCESS;
    }
    if (listener_created) {
        cleanup_ok &= clm_net_socket_close(listener_id) == CLM_NET_SUCCESS;
    }
    CLM_ASSERT(cleanup_ok &&
        clm_net_socket_close(86199) == CLM_NET_ERROR_NOT_FOUND &&
        clm_net_finalize() == CLM_NET_SUCCESS &&
        clm_net_finalize() == CLM_NET_ERROR_NOT_INITIALIZED &&
        clm_net_initialize() == CLM_NET_SUCCESS &&
        clm_net_finalize() == CLM_NET_SUCCESS,
        "close registered sockets, reject unknown IDs, and finalize cleanly");

    int strings_cleaned = 1;
    if (payload_string_created) {
        strings_cleaned &= clm_string_delete(payload_string_id);
    }
    if (formatted_string_created) {
        strings_cleaned &= clm_string_delete(formatted_string_id);
    }
    if (host_string_created) {
        strings_cleaned &= clm_string_delete(host_string_id);
    }
    CLM_ASSERT(strings_cleaned, "delete all CLM string-ID networking fixtures");

    clm_test_summary(result.passed, result.failed);
    return result.failed != 0;
}

/* Creates a bound nonblocking loopback UDP socket and reports its endpoint. */
static int clm_net_poller_test_udp(clm_id socket_id, clm_net_address* address)
{
    clm_net_address loopback = {0};
    return clm_net_socket_create(socket_id, CLM_NET_IPV4, CLM_NET_UDP) ==
            CLM_NET_SUCCESS &&
        clm_net_socket_set_nonblocking(socket_id, true) == CLM_NET_SUCCESS &&
        clm_net_address_parse("127.0.0.1", 0, &loopback) == CLM_NET_SUCCESS &&
        clm_net_bind(socket_id, &loopback) == CLM_NET_SUCCESS &&
        clm_net_socket_local_address(socket_id, address) == CLM_NET_SUCCESS;
}

static int clm_net_poller_test_ping(clm_id sender_id,
    const clm_net_address* target)
{
    size_t sent = 0;
    return clm_net_send_to(sender_id, target, "x", 1, &sent) ==
            CLM_NET_SUCCESS && sent == 1;
}

static int clm_net_poller_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Net poller");

    enum { PEERS = 100 };
    const clm_id base_id = 87000;
    const clm_id sender_id = 86999;
    CLM_ASSERT(clm_net_initialize() == CLM_NET_SUCCESS,
        "initialize networking for the poller tests");

    clm_net_poller* poller = NULL;
    size_t count = 99;
    clm_net_poller_event events[8];
    CLM_ASSERT(clm_net_poller_create(NULL) == CLM_NET_ERROR_INVALID_ARGUMENT &&
        clm_net_poller_create(&poller) == CLM_NET_SUCCESS && poller != NULL &&
        clm_net_poller_count(poller) == 0 &&
        clm_net_poller_backend() != NULL,
        "create an empty poller and name its backend");
    CLM_ASSERT(poller != NULL &&
        clm_net_poller_wait(poller, events, 8, CLM_NET_WAIT_FOREVER, &count) ==
            CLM_NET_ERROR_INVALID_ARGUMENT && count == 99 &&
        clm_net_poller_wait(poller, events, 8, 5, &count) == CLM_NET_TIMEOUT &&
        count == 0 &&
        clm_net_poller_wait(poller, events, 0, 5, &count) ==
            CLM_NET_ERROR_INVALID_ARGUMENT,
        "reject an unbounded wait on an empty poller and time out otherwise");

    clm_net_address sender_address = {0};
    clm_net_address addresses[PEERS];
    int peers_ready = clm_net_poller_test_udp(sender_id, &sender_address);
    for (int index = 0; index < PEERS; ++index) {
        peers_ready &= clm_net_poller_test_udp(base_id + (clm_id)index,
            &addresses[index]);
    }
    CLM_ASSERT(peers_ready, "create more UDP sockets than select() can watch");

    int all_registered = poller != NULL && peers_ready;
    for (int index = 0; all_registered && index < PEERS; ++index) {
        all_registered = clm_net_poller_set(poller, base_id + (clm_id)index,
            CLM_NET_EVENT_READ, &addresses[index]) == CLM_NET_SUCCESS;
    }
    CLM_ASSERT(all_registered && clm_net_poller_count(poller) == PEERS &&
        clm_net_poller_set(poller, 86998, CLM_NET_EVENT_READ, NULL) ==
            CLM_NET_ERROR_NOT_FOUND &&
        clm_net_poller_set(poller, base_id, 0, NULL) ==
            CLM_NET_ERROR_INVALID_ARGUMENT &&
        clm_net_poller_set(poller, base_id, 0x40u, NULL) ==
            CLM_NET_ERROR_INVALID_ARGUMENT &&
        clm_net_poller_count(poller) == PEERS,
        "register 100 sockets and reject unknown sockets and bad interests");

    CLM_ASSERT(all_registered &&
        clm_net_poller_wait(poller, events, 8, 20, &count) ==
            CLM_NET_TIMEOUT && count == 0,
        "report no events while nothing is ready");

    int found = all_registered && clm_net_poller_test_ping(sender_id,
        &addresses[80]) &&
        clm_net_poller_wait(poller, events, 8, 1000, &count) ==
            CLM_NET_SUCCESS && count == 1 &&
        events[0].socket_id == base_id + 80 &&
        events[0].user_data == &addresses[80] &&
        (events[0].revents & CLM_NET_EVENT_READ) != 0;
    CLM_ASSERT(found,
        "report only the ready socket among 100 with its user data");

    count = 0;
    CLM_ASSERT(found &&
        clm_net_poller_wait(poller, events, 8, 1000, &count) ==
            CLM_NET_SUCCESS && count == 1 &&
        events[0].socket_id == base_id + 80,
        "report an unread socket again on the next wait (level-triggered)");

    char sink[4];
    size_t received = 0;
    CLM_ASSERT(found &&
        clm_net_receive_from(base_id + 80, sink, sizeof(sink), &received,
            NULL) == CLM_NET_SUCCESS && received == 1 &&
        clm_net_poller_wait(poller, events, 8, 20, &count) ==
            CLM_NET_TIMEOUT && count == 0,
        "stop reporting a socket once its data has been read");

    /* Removing half the sockets exercises hash-table backward-shift deletion;
     * every survivor must remain reachable and every victim must be silent. */
    int removed_ok = all_registered;
    for (int index = 1; removed_ok && index < PEERS; index += 2) {
        removed_ok = clm_net_poller_remove(poller, base_id + (clm_id)index) ==
            CLM_NET_SUCCESS;
    }
    CLM_ASSERT(removed_ok && clm_net_poller_count(poller) == PEERS / 2 &&
        clm_net_poller_remove(poller, base_id + 1) == CLM_NET_ERROR_NOT_FOUND,
        "remove registered sockets and report NOT_FOUND for absent ones");

    int survivors_ok = removed_ok;
    for (int index = 0; survivors_ok && index < PEERS; index += 2) {
        survivors_ok = clm_net_poller_test_ping(sender_id, &addresses[index]) &&
            clm_net_poller_wait(poller, events, 8, 1000, &count) ==
                CLM_NET_SUCCESS && count == 1 &&
            events[0].socket_id == base_id + (clm_id)index &&
            clm_net_receive_from(base_id + (clm_id)index, sink, sizeof(sink),
                &received, NULL) == CLM_NET_SUCCESS;
    }
    CLM_ASSERT(survivors_ok,
        "keep every surviving socket reachable after other sockets are removed");

    CLM_ASSERT(removed_ok && clm_net_poller_test_ping(sender_id,
            &addresses[1]) &&
        clm_net_poller_wait(poller, events, 8, 50, &count) ==
            CLM_NET_TIMEOUT && count == 0 &&
        clm_net_receive_from(base_id + 1, sink, sizeof(sink), &received,
            NULL) == CLM_NET_SUCCESS,
        "stay silent for a socket that was removed from the poller");

    /* More ready sockets than output slots: nothing is lost across waits. */
    int limited_ok = removed_ok;
    for (int index = 0; limited_ok && index < 6; index += 2) {
        limited_ok = clm_net_poller_test_ping(sender_id, &addresses[index]);
    }
    bool seen[3] = {false, false, false};
    for (int attempt = 0; limited_ok && attempt < 6; ++attempt) {
        limited_ok = clm_net_poller_wait(poller, events, 2, 1000, &count) ==
            CLM_NET_SUCCESS && count >= 1 && count <= 2;
        for (size_t slot = 0; limited_ok && slot < count; ++slot) {
            clm_id index = events[slot].socket_id - base_id;
            if (index == 0 || index == 2 || index == 4) {
                seen[index / 2] = true;
            }
        }
    }
    CLM_ASSERT(limited_ok && seen[0] && seen[1] && seen[2],
        "honor a small output capacity without starving any ready socket");
    for (int index = 0; removed_ok && index < 6; index += 2) {
        (void)clm_net_receive_from(base_id + (clm_id)index, sink, sizeof(sink),
            &received, NULL);
    }

    /* Interest changes and user-data updates on a live registration. */
    int modified = removed_ok &&
        clm_net_poller_set(poller, base_id, CLM_NET_EVENT_WRITE, &count) ==
            CLM_NET_SUCCESS &&
        clm_net_poller_wait(poller, events, 8, 1000, &count) ==
            CLM_NET_SUCCESS && count >= 1;
    bool wrote = false;
    for (size_t slot = 0; modified && slot < count; ++slot) {
        wrote |= events[slot].socket_id == base_id &&
            (events[slot].revents & CLM_NET_EVENT_WRITE) != 0 &&
            events[slot].user_data == &count;
    }
    CLM_ASSERT(modified && wrote,
        "switch a registration to WRITE interest and replace its user data");

    /* An ID that was closed and recreated maps to a new native socket. */
    clm_net_address recreated_address = {0};
    int recreated = removed_ok &&
        clm_net_poller_set(poller, base_id, CLM_NET_EVENT_READ, NULL) ==
            CLM_NET_SUCCESS &&
        clm_net_poller_remove(poller, base_id) == CLM_NET_SUCCESS &&
        clm_net_poller_set(poller, base_id + 2, CLM_NET_EVENT_READ, NULL) ==
            CLM_NET_SUCCESS &&
        clm_net_socket_close(base_id + 2) == CLM_NET_SUCCESS &&
        clm_net_poller_test_udp(base_id + 2, &recreated_address) &&
        clm_net_poller_set(poller, base_id + 2, CLM_NET_EVENT_READ, NULL) ==
            CLM_NET_SUCCESS &&
        clm_net_poller_test_ping(sender_id, &recreated_address);
    int recreated_seen = 0;
    for (int attempt = 0; recreated && attempt < 3 && !recreated_seen;
         ++attempt) {
        if (clm_net_poller_wait(poller, events, 8, 1000, &count) ==
                CLM_NET_SUCCESS) {
            for (size_t slot = 0; slot < count; ++slot) {
                recreated_seen |= events[slot].socket_id == base_id + 2 &&
                    (events[slot].revents & CLM_NET_EVENT_READ) != 0;
            }
        }
    }
    CLM_ASSERT(recreated && recreated_seen,
        "re-register a socket ID that was closed and recreated");

    int closed_ok = 1;
    for (int index = 0; index < PEERS; ++index) {
        /* Unregistered, registered and already-closed sockets all close. */
        clm_net_status status = clm_net_socket_close(base_id + (clm_id)index);
        closed_ok &= status == CLM_NET_SUCCESS ||
            status == CLM_NET_ERROR_NOT_FOUND;
    }
    closed_ok &= clm_net_socket_close(sender_id) == CLM_NET_SUCCESS;
    CLM_ASSERT(closed_ok, "close sockets that are still registered");

    clm_net_poller_destroy(poller);
    clm_net_poller_destroy(NULL);
    CLM_ASSERT(clm_net_finalize() == CLM_NET_SUCCESS,
        "destroy the poller and finalize networking");

    clm_test_summary(result.passed, result.failed);
    return result.failed != 0;
}
