/* Copyright (c) 2019-2022 Stanford University
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "homa_impl.h"
#define KSELFTEST_NOT_MAIN 1
#include "kselftest_harness.h"
#include "ccutils.h"
#include "mock.h"
#include "utils.h"

extern struct homa *homa;

FIXTURE(homa_plumbing) {
	struct in6_addr client_ip[1];
	int client_port;
	struct in6_addr server_ip[1];
	int server_port;
	__u64 client_id;
	__u64 server_id;
	struct homa homa;
	struct homa_sock hsk;
	sockaddr_in_union client_addr;
	sockaddr_in_union server_addr;
	struct data_header data;
	int starting_skb_count;
	struct iovec reply_vec[2];
	struct iovec send_vec[2];
	struct homa_recv_args recv_args;
	struct homa_reply_args reply_args;
	struct homa_send_args send_args;
	struct iovec recv_vec[2];
	char buffer[2000];
};
FIXTURE_SETUP(homa_plumbing)
{
	self->client_ip[0] = unit_get_in_addr("196.168.0.1");
	self->client_port = 40000;
	self->server_ip[0] = unit_get_in_addr("1.2.3.4");
	self->server_port = 99;
	self->client_id = 1234;
	self->server_id = 1235;
	self->client_addr.in6.sin6_addr = self->client_ip[0];
	self->client_addr.in6.sin6_port = htons(self->client_port);
	self->server_addr.in6.sin6_addr = self->server_ip[0];
	self->server_addr.in6.sin6_port = htons(self->server_port);
	homa = &self->homa;
	homa_init(&self->homa);
	mock_sock_init(&self->hsk, &self->homa, 0);
	self->client_addr.in6.sin6_family = self->hsk.inet.sk.sk_family;
	self->server_addr.in6.sin6_family = self->hsk.inet.sk.sk_family;
	if (self->hsk.inet.sk.sk_family == AF_INET) {
		self->client_addr.in4.sin_addr.s_addr =
			ipv6_to_ipv4(self->client_addr.in6.sin6_addr);
		self->server_addr.in4.sin_addr.s_addr =
			ipv6_to_ipv4(self->server_addr.in6.sin6_addr);
	}
	homa_sock_bind(&self->homa.port_map, &self->hsk, self->server_port);
	self->data = (struct data_header){.common = {
			.sport = htons(self->client_port),
	                .dport = htons(self->server_port),
			.type = DATA,
			.sender_id = cpu_to_be64(self->client_id)},
			.message_length = htonl(10000),
			.incoming = htonl(10000), .retransmit = 0,
			.seg={.offset = 0}};
	self->recv_args.message_buf = self->buffer;
	self->recv_args.iovec = NULL;
	self->recv_args.length = sizeof(self->buffer);
	self->recv_args.flags = HOMA_RECV_RESPONSE;
	self->recv_args.id = 0;
	self->reply_vec[0].iov_base = self->buffer;
	self->reply_vec[0].iov_len = 100;
	self->reply_vec[1].iov_base = self->buffer + 1000;
	self->reply_vec[1].iov_len = 900;
	self->reply_args.message_buf = NULL;
	self->reply_args.iovec = self->reply_vec;
	self->reply_args.length = 2;
	self->reply_args.dest_addr = self->client_addr;
	self->reply_args.id = self->server_id;
	self->send_vec[0].iov_base = self->buffer;
	self->send_vec[0].iov_len = 100;
	self->send_vec[1].iov_base = self->buffer + 1000;
	self->send_vec[1].iov_len = 100;
	self->send_args.message_buf = NULL;
	self->send_args.iovec = self->send_vec;
	self->send_args.length = 2;
	self->send_args.dest_addr = self->server_addr;
	self->send_args.id = 0;
	self->recv_vec[0].iov_base = (void *) 10000;
	self->recv_vec[0].iov_len = 60;
	self->recv_vec[1].iov_base = (void *)
			(10000 + self->recv_vec[0].iov_len);
	self->recv_vec[1].iov_len = sizeof(self->buffer) -
			self->recv_vec[0].iov_len;
	unit_log_clear();
}
FIXTURE_TEARDOWN(homa_plumbing)
{
	homa_destroy(&self->homa);
	unit_teardown();
	homa = NULL;
}

TEST_F(homa_plumbing, homa_bind__version_mismatch)
{
	// Make sure the test uses IPv4.
	mock_ipv6 = false;
	homa_sock_destroy(&self->hsk);
	mock_sock_init(&self->hsk, &self->homa, 0);

	struct sockaddr addr = {};
	addr.sa_family = AF_INET6;
	struct socket sock = {};
	sock.sk = &self->hsk.inet.sk;
	int result = homa_bind(&sock, &addr, sizeof(addr));
	EXPECT_EQ(EAFNOSUPPORT, -result);
}
TEST_F(homa_plumbing, homa_bind__ipv6_address_too_short)
{
	// Make sure the test uses IPv6.
	mock_ipv6 = true;
	homa_sock_destroy(&self->hsk);
	mock_sock_init(&self->hsk, &self->homa, 0);

	sockaddr_in_union addr = {};
	addr.in6.sin6_family = AF_INET6;
	struct socket sock = {};
	sock.sk = &self->hsk.inet.sk;
	int result = homa_bind(&sock, &addr.sa, sizeof(addr.in6)-1);
	EXPECT_EQ(EINVAL, -result);
}
TEST_F(homa_plumbing, homa_bind__ipv6_ok)
{
	// Make sure the test uses IPv6.
	mock_ipv6 = true;
	homa_sock_destroy(&self->hsk);
	mock_sock_init(&self->hsk, &self->homa, 0);

	sockaddr_in_union addr = {};
	addr.in6.sin6_family = AF_INET6;
	addr.in6.sin6_port = htons(123);
	struct socket sock = {};
	sock.sk = &self->hsk.inet.sk;
	int result = homa_bind(&sock, &addr.sa, sizeof(addr.in6));
	EXPECT_EQ(0, -result);
	EXPECT_EQ(123, self->hsk.port);
}
TEST_F(homa_plumbing, homa_bind__ipv4_address_too_short)
{
	// Make sure the test uses IPv4.
	mock_ipv6 = false;
	homa_sock_destroy(&self->hsk);
	mock_sock_init(&self->hsk, &self->homa, 0);

	sockaddr_in_union addr = {};
	addr.in4.sin_family = AF_INET;
	struct socket sock = {};
	sock.sk = &self->hsk.inet.sk;
	int result = homa_bind(&sock, &addr.sa, sizeof(addr.in4)-1);
	EXPECT_EQ(EINVAL, -result);
}
TEST_F(homa_plumbing, homa_bind__ipv4_ok)
{
	// Make sure the test uses IPv4.
	mock_ipv6 = false;
	homa_sock_destroy(&self->hsk);
	mock_sock_init(&self->hsk, &self->homa, 0);

	sockaddr_in_union addr = {};
	addr.in4.sin_family = AF_INET;
	addr.in4.sin_port = htons(345);
	struct socket sock = {};
	sock.sk = &self->hsk.inet.sk;
	int result = homa_bind(&sock, &addr.sa, sizeof(addr.in4));
	EXPECT_EQ(0, -result);
	EXPECT_EQ(345, self->hsk.port);
}

TEST_F(homa_plumbing, homa_ioc_recv__cant_read_user_args)
{
	mock_copy_data_errors = 1;
	EXPECT_EQ(EFAULT, -homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(0LU, self->recv_args.id);
}
TEST_F(homa_plumbing, homa_ioc_recv__socket_shutdown)
{
	self->hsk.shutdown = true;
	EXPECT_EQ(ESHUTDOWN, -homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	self->hsk.shutdown = false;
}
TEST_F(homa_plumbing, homa_ioc_recv__use_iovec)
{
	unit_client_rpc(&self->hsk, RPC_READY, self->client_ip, self->server_ip,
			self->server_port, self->client_id, 100, 200);

	self->recv_args.message_buf = NULL;
	self->recv_args.iovec = self->recv_vec;
	self->recv_args.length = 2;
	self->recv_args.flags = HOMA_RECV_NONBLOCKING|HOMA_RECV_RESPONSE;
	unit_log_clear();
	EXPECT_EQ(200, homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(200, self->recv_args.length);
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
	EXPECT_SUBSTR("skb_copy_datagram_iter: 60 bytes to 10000: 0-59; "
		"skb_copy_datagram_iter: 140 bytes to 10060: 60-199",
		unit_log_get());
}
TEST_F(homa_plumbing, homa_ioc_recv__error_in_import_iovec)
{
	unit_client_rpc(&self->hsk, RPC_READY, self->client_ip, self->server_ip,
			self->server_port, self->client_id, 100, 200);

	self->recv_args.message_buf = NULL;
	self->recv_args.iovec = self->recv_vec;
	self->recv_args.length = 2;
	self->recv_args.flags = HOMA_RECV_NONBLOCKING|HOMA_RECV_RESPONSE;
	unit_log_clear();
	mock_import_iovec_errors = 1;
	EXPECT_EQ(EINVAL, -homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(1, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_recv__error_in_homa_wait_for_message)
{
	self->recv_args.flags = HOMA_RECV_NONBLOCKING|HOMA_RECV_RESPONSE;
	EXPECT_EQ(EAGAIN, -homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(0LU, self->recv_args.id);
}
TEST_F(homa_plumbing, homa_ioc_recv__HOMA_RECV_PARTIAL)
{
	unit_client_rpc(&self->hsk, RPC_READY, self->client_ip, self->server_ip,
			self->server_port, self->client_id, 100, 200);
	self->recv_args.flags = HOMA_RECV_NONBLOCKING|HOMA_RECV_RESPONSE
			|HOMA_RECV_PARTIAL;

	// First call gets most of message.
	self->recv_args.length = 150;
	EXPECT_EQ(150, homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(self->client_id, self->recv_args.id);
	EXPECT_EQ(1, unit_list_length(&self->hsk.active_rpcs));

	// Second call gets remainder, deletes message.
	self->recv_args.length = 200;
	memset(&self->recv_args.source_addr, 0, sizeof(self->recv_args.source_addr));
	self->recv_args.id = self->client_id;
	EXPECT_EQ(50, homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(self->client_id, self->recv_args.id);
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_recv__free_after_error_with_partial)
{
	struct homa_rpc *crpc = unit_client_rpc(&self->hsk,
			RPC_READY, self->client_ip, self->server_ip,
			self->server_port, self->client_id, 100, 200);
	ASSERT_NE(NULL, crpc);
	crpc->error = -ETIMEDOUT;
	self->recv_args.length = 100;
	self->recv_args.flags = HOMA_RECV_NONBLOCKING|HOMA_RECV_RESPONSE
			|HOMA_RECV_PARTIAL;
	EXPECT_EQ(ETIMEDOUT, -homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(self->client_id, self->recv_args.id);
	if (self->hsk.inet.sk.sk_family == AF_INET6) {
		EXPECT_EQ_IP(*self->server_ip,
				self->recv_args.source_addr.in6.sin6_addr);
	} else {
		EXPECT_EQ_IP(*self->server_ip,
				ipv4_to_ipv6(self->recv_args.source_addr.in4
				.sin_addr.s_addr));
	}
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_recv__rpc_has_error)
{
	struct homa_rpc *crpc = unit_client_rpc(&self->hsk,
			RPC_READY, self->client_ip, self->server_ip,
			self->server_port, self->client_id, 100, 200);
	ASSERT_NE(NULL, crpc);
	crpc->error = -ETIMEDOUT;
	self->recv_args.flags = HOMA_RECV_NONBLOCKING|HOMA_RECV_RESPONSE;
	EXPECT_EQ(ETIMEDOUT, -homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(self->client_id, self->recv_args.id);
	if (self->hsk.inet.sk.sk_family == AF_INET6) {
		EXPECT_EQ_IP(*self->server_ip,
				self->recv_args.source_addr.in6.sin6_addr);
	} else {
		EXPECT_EQ_IP(*self->server_ip,
				ipv4_to_ipv6(self->recv_args.source_addr.in4
				.sin_addr.s_addr));
	}
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_recv__cant_update_user_arguments)
{
	unit_client_rpc(&self->hsk, RPC_READY, self->client_ip, self->server_ip,
			self->server_port, self->client_id, 100, 200);
	self->recv_args.flags = HOMA_RECV_NONBLOCKING|HOMA_RECV_RESPONSE;
	mock_copy_to_user_errors = 1;
	EXPECT_EQ(EFAULT, -homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_recv__client_normal_completion_ipv4)
{
	// Make sure the test uses IPv4.
	mock_ipv6 = false;
	homa_sock_destroy(&self->hsk);
	mock_sock_init(&self->hsk, &self->homa, 0);

	unit_client_rpc(&self->hsk, RPC_READY, self->client_ip, self->server_ip,
			self->server_port, self->client_id, 100, 200);
	self->recv_args.flags = HOMA_RECV_NONBLOCKING|HOMA_RECV_RESPONSE;
	EXPECT_EQ(200, homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(200, self->recv_args.length);
	EXPECT_EQ(self->client_id, self->recv_args.id);
	EXPECT_EQ(AF_INET, self->recv_args.source_addr.in4.sin_family);
	EXPECT_STREQ("1.2.3.4", homa_print_ipv4_addr(
			self->recv_args.source_addr.in4.sin_addr.s_addr));
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_recv__client_normal_completion_ipv6)
{
	// Make sure the test uses IPv6.
	mock_ipv6 = true;
	homa_sock_destroy(&self->hsk);
	mock_sock_init(&self->hsk, &self->homa, 0);
	struct in6_addr server_ip6 = unit_get_in_addr("1::3:5:7");

	unit_client_rpc(&self->hsk, RPC_READY, self->client_ip, &server_ip6,
			self->server_port, self->client_id, 100, 200);
	self->recv_args.flags = HOMA_RECV_NONBLOCKING|HOMA_RECV_RESPONSE;
	EXPECT_EQ(200, homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(200, self->recv_args.length);
	EXPECT_EQ(self->client_id, self->recv_args.id);
	EXPECT_EQ(AF_INET6, self->recv_args.source_addr.in6.sin6_family);
	EXPECT_STREQ("[1::3:5:7]", homa_print_ipv6_addr(
			&self->recv_args.source_addr.in6.sin6_addr));
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_recv__completion_cookie)
{
	const uint64_t oatmeal = 0x6F61746D65616C00;
	const uint64_t raisins = 0x72616973696E7300;
	uint64_t best_cookie = oatmeal + raisins;
	struct homa_rpc *crpc = unit_client_rpc(&self->hsk, RPC_READY,
			self->client_ip, self->server_ip, self->server_port,
			self->client_id, 100, 200);
	crpc->completion_cookie = best_cookie;
	self->recv_args.flags = HOMA_RECV_NONBLOCKING|HOMA_RECV_RESPONSE;
	EXPECT_EQ(200, homa_ioc_recv(&self->hsk.inet.sk,
		(unsigned long) &self->recv_args));
	EXPECT_EQ(self->client_id, self->recv_args.id);
	EXPECT_EQ(best_cookie, self->recv_args.completion_cookie);
}
TEST_F(homa_plumbing, homa_ioc_recv__server_normal_completion)
{
	struct homa_rpc *srpc = unit_server_rpc(&self->hsk, RPC_READY,
			self->client_ip, self->server_ip, self->client_port,
		        self->server_id, 100, 200);
	self->recv_args.flags = HOMA_RECV_NONBLOCKING|HOMA_RECV_REQUEST;
	EXPECT_EQ(100, homa_ioc_recv(&self->hsk.inet.sk,
			(unsigned long) &self->recv_args));
	EXPECT_EQ(100, self->recv_args.length);
	EXPECT_EQ(self->server_id, self->recv_args.id);
	if (self->hsk.inet.sk.sk_family == AF_INET6) {
		EXPECT_EQ_IP(*self->client_ip,
				self->recv_args.source_addr.in6.sin6_addr);
	} else {
		EXPECT_EQ_IP(*self->client_ip,
				ipv4_to_ipv6(self->recv_args.source_addr.in4
				.sin_addr.s_addr));
	}
	EXPECT_EQ(1, unit_list_length(&self->hsk.active_rpcs));
	EXPECT_EQ(RPC_IN_SERVICE, srpc->state);
}

TEST_F(homa_plumbing, homa_ioc_reply__basics)
{
	struct homa_rpc *srpc = unit_server_rpc(&self->hsk, RPC_IN_SERVICE,
			self->client_ip, self->server_ip, self->client_port,
		        self->server_id, 2000, 100);
	unit_log_clear();
	EXPECT_EQ(0, -homa_ioc_reply(&self->hsk.inet.sk,
			(unsigned long) &self->reply_args));
	EXPECT_NE(RPC_IN_SERVICE, srpc->state);
	EXPECT_SUBSTR("xmit DATA 1000@0", unit_log_get());
}
TEST_F(homa_plumbing, homa_ioc_reply__cant_read_user_args)
{
	mock_copy_data_errors = 1;
	EXPECT_EQ(EFAULT, -homa_ioc_reply(&self->hsk.inet.sk,
			(unsigned long) &self->reply_args));
}
TEST_F(homa_plumbing, homa_ioc_reply__bad_address_family)
{
	struct homa_rpc *srpc = unit_server_rpc(&self->hsk, RPC_IN_SERVICE,
			self->client_ip, self->server_ip, self->client_port,
		        self->server_id, 2000, 100);
	unit_log_clear();
	int family = (self->hsk.inet.sk.sk_family == AF_INET) ? AF_INET6
			: AF_INET;
	self->reply_args.dest_addr.in6.sin6_family = family;
	EXPECT_EQ(EAFNOSUPPORT, -homa_ioc_reply(&self->hsk.inet.sk,
			(unsigned long) &self->reply_args));
	EXPECT_EQ(RPC_IN_SERVICE, srpc->state);
}
TEST_F(homa_plumbing, homa_ioc_reply__error_in_import_iovec)
{
	struct homa_rpc *srpc = unit_server_rpc(&self->hsk, RPC_IN_SERVICE,
			self->client_ip, self->server_ip, self->client_port,
		        self->server_id, 2000, 100);
	unit_log_clear();
	mock_import_iovec_errors = 1;
	EXPECT_EQ(EINVAL, -homa_ioc_reply(&self->hsk.inet.sk,
			(unsigned long) &self->reply_args));
	EXPECT_EQ(RPC_IN_SERVICE, srpc->state);
	EXPECT_EQ(1, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_reply__cant_find_rpc)
{
	struct homa_rpc *srpc = unit_server_rpc(&self->hsk, RPC_IN_SERVICE,
			self->client_ip, self->server_ip, self->client_port,
		        self->server_id, 2000, 100);
	unit_log_clear();
	self->reply_args.id += 1;
	EXPECT_EQ(EINVAL, -homa_ioc_reply(&self->hsk.inet.sk,
			(unsigned long) &self->reply_args));
	EXPECT_EQ(RPC_IN_SERVICE, srpc->state);
}
TEST_F(homa_plumbing, homa_ioc_reply__error_in_homa_message_out_init)
{
	struct homa_rpc *srpc = unit_server_rpc(&self->hsk, RPC_IN_SERVICE,
			self->client_ip, self->server_ip, self->client_port,
		        self->server_id, 2000, 100);
	unit_log_clear();
	mock_alloc_skb_errors = 1;
	EXPECT_EQ(ENOMEM, -homa_ioc_reply(&self->hsk.inet.sk,
			(unsigned long) &self->reply_args));
	EXPECT_EQ(RPC_IN_SERVICE, srpc->state);
	EXPECT_EQ(1, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_reply__dont_free_rpc)
{
	struct homa_rpc *srpc = unit_server_rpc(&self->hsk, RPC_IN_SERVICE,
			self->client_ip, self->server_ip, self->client_port,
		        self->server_id, 2000, 100);
	unit_log_clear();
	self->reply_args.length = 10000;
	self->reply_args.iovec = NULL;
	self->reply_args.message_buf = (void *) 1000;
	self->homa.rtt_bytes = 5000;
	EXPECT_EQ(0, -homa_ioc_reply(&self->hsk.inet.sk,
			(unsigned long) &self->reply_args));
	EXPECT_EQ(RPC_OUTGOING, srpc->state);
	EXPECT_SUBSTR("xmit DATA 1400@0; xmit DATA 1400@1400",
			unit_log_get());
	EXPECT_EQ(1, unit_list_length(&self->hsk.active_rpcs));
}

TEST_F(homa_plumbing, homa_ioc_send__cant_read_user_args)
{
	mock_copy_data_errors = 1;
	EXPECT_EQ(EFAULT, -homa_ioc_send(&self->hsk.inet.sk,
		(unsigned long) &self->send_args));
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_send__bad_address_family)
{
	int family = (self->hsk.inet.sk.sk_family == AF_INET) ? AF_INET6
			: AF_INET;
	self->send_args.dest_addr.in6.sin6_family = family;
	EXPECT_EQ(EAFNOSUPPORT, -homa_ioc_send(&self->hsk.inet.sk,
			(unsigned long) &self->send_args));
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_send__error_in_import_iovec)
{
	mock_import_iovec_errors = 1;
	EXPECT_EQ(EINVAL, -homa_ioc_send(&self->hsk.inet.sk,
			(unsigned long) &self->send_args));
}
TEST_F(homa_plumbing, homa_ioc_send__error_in_homa_rpc_new_client)
{
	mock_kmalloc_errors = 2;
	EXPECT_EQ(ENOMEM, -homa_ioc_send(&self->hsk.inet.sk,
			(unsigned long) &self->send_args));
}
TEST_F(homa_plumbing, homa_ioc_send__cant_update_user_arguments)
{
	mock_copy_to_user_errors = 1;
	atomic64_set(&self->homa.next_outgoing_id, 1234);
	EXPECT_EQ(EFAULT, -homa_ioc_send(&self->hsk.inet.sk,
			(unsigned long) &self->send_args));
	EXPECT_SUBSTR("xmit DATA 200@0", unit_log_get());
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_send__successful_send)
{
	atomic64_set(&self->homa.next_outgoing_id, 1234);
	EXPECT_EQ(0, homa_ioc_send(&self->hsk.inet.sk,
			(unsigned long) &self->send_args));
	EXPECT_SUBSTR("xmit DATA 200@0", unit_log_get());
	EXPECT_EQ(1234L, self->send_args.id);
	EXPECT_EQ(1, unit_list_length(&self->hsk.active_rpcs));
}

TEST_F(homa_plumbing, homa_ioc_abort__basics)
{
	struct homa_abort_args args = {self->client_id, 0};
	struct homa_rpc *crpc = unit_client_rpc(&self->hsk,
			RPC_OUTGOING, self->client_ip, self->server_ip,
			self->server_port, self->client_id, 10000, 200);
	ASSERT_NE(NULL, crpc);
	EXPECT_EQ(0, homa_ioc_abort(&self->hsk.inet.sk, (unsigned long) &args));
	EXPECT_EQ(RPC_DEAD, crpc->state);
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_abort__cant_read_user_args)
{
	struct homa_abort_args args = {self->client_id, 0};
	mock_copy_data_errors = 1;
	EXPECT_EQ(EFAULT, -homa_ioc_abort(&self->hsk.inet.sk,
			(unsigned long) &args));
}
TEST_F(homa_plumbing, homa_ioc_abort__abort_multiple_rpcs)
{
	struct homa_abort_args args = {0, ECANCELED};
	struct homa_rpc *crpc1 = unit_client_rpc(&self->hsk,
			RPC_OUTGOING, self->client_ip, self->server_ip,
			self->server_port, self->client_id, 10000, 200);
	struct homa_rpc *crpc2 = unit_client_rpc(&self->hsk,
			RPC_OUTGOING, self->client_ip, self->server_ip,
			self->server_port, self->client_id, 10000, 200);
	ASSERT_NE(NULL, crpc1);
	ASSERT_NE(NULL, crpc2);
	EXPECT_EQ(0, homa_ioc_abort(&self->hsk.inet.sk, (unsigned long) &args));
	EXPECT_EQ(RPC_READY, crpc1->state);
	EXPECT_EQ(-ECANCELED, crpc1->error);
	EXPECT_EQ(RPC_READY, crpc2->state);
	EXPECT_EQ(-ECANCELED, crpc2->error);
	EXPECT_EQ(2, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_ioc_abort__complete_rpc)
{
	struct homa_abort_args args = {self->client_id, ECANCELED};
	struct homa_rpc *crpc = unit_client_rpc(&self->hsk,
			RPC_OUTGOING, self->client_ip, self->server_ip,
			self->server_port, self->client_id, 10000, 200);
	ASSERT_NE(NULL, crpc);
	EXPECT_EQ(0, homa_ioc_abort(&self->hsk.inet.sk, (unsigned long) &args));
	EXPECT_EQ(RPC_READY, crpc->state);
	EXPECT_EQ(-ECANCELED, crpc->error);
	EXPECT_EQ(1, unit_list_length(&self->hsk.active_rpcs));

	// Try again, now that the RPC is already finished.
	EXPECT_EQ(-EALREADY, homa_ioc_abort(&self->hsk.inet.sk,
			(unsigned long) &args));
}
TEST_F(homa_plumbing, homa_ioc_abort__nonexistent_rpc)
{
	struct homa_abort_args args = {99, 0};
	EXPECT_EQ(EINVAL, -homa_ioc_abort(&self->hsk.inet.sk,
			(unsigned long) &args));
}

TEST_F(homa_plumbing, homa_softirq__basics)
{
	struct sk_buff *skb;
	skb = mock_skb_new(self->client_ip, &self->data.common, 1400, 1400);
	homa_softirq(skb);
	EXPECT_EQ(1, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_softirq__reorder_incoming_packets)
{
	struct sk_buff *skb, *skb2, *skb3, *skb4;

	self->data.common.sender_id = cpu_to_be64(2000);
	self->data.message_length = htonl(2000);
	skb = mock_skb_new(self->client_ip, &self->data.common, 1400, 0);
	self->data.common.sender_id = cpu_to_be64(200);
	self->data.message_length = htonl(200);
	skb2 = mock_skb_new(self->client_ip, &self->data.common, 200, 0);
	self->data.common.sender_id = cpu_to_be64(300);
	self->data.message_length = htonl(300);
	skb3 = mock_skb_new(self->client_ip, &self->data.common, 300, 0);
	self->data.common.sender_id = cpu_to_be64(5000);
	self->data.message_length = htonl(5000);
	skb4 = mock_skb_new(self->client_ip, &self->data.common, 1400, 0);
	skb_shinfo(skb)->frag_list = skb2;
	skb2->next = skb3;
	skb3->next = skb4;
	skb4->next = NULL;
	homa_softirq(skb);
	unit_log_active_ids(&self->hsk);
	EXPECT_STREQ("201 301 2001 5001", unit_log_get());
}
TEST_F(homa_plumbing, homa_softirq__reorder_short_packet_at_front)
{
	struct sk_buff *skb, *skb2, *skb3, *skb4;

	self->data.common.sender_id = cpu_to_be64(200);
	self->data.message_length = htonl(200);
	skb = mock_skb_new(self->client_ip, &self->data.common, 200, 0);
	self->data.common.sender_id = cpu_to_be64(4000);
	self->data.message_length = htonl(4000);
	skb2 = mock_skb_new(self->client_ip, &self->data.common, 1400, 0);
	self->data.common.sender_id = cpu_to_be64(300);
	self->data.message_length = htonl(300);
	skb3 = mock_skb_new(self->client_ip, &self->data.common, 300, 0);
	self->data.common.sender_id = cpu_to_be64(5000);
	self->data.message_length = htonl(5000);
	skb4 = mock_skb_new(self->client_ip, &self->data.common, 1400, 0);
	skb_shinfo(skb)->frag_list = skb2;
	skb2->next = skb3;
	skb3->next = skb4;
	skb4->next = NULL;
	homa_softirq(skb);
	unit_log_active_ids(&self->hsk);
	EXPECT_STREQ("201 301 4001 5001", unit_log_get());
}
TEST_F(homa_plumbing, homa_softirq__nothing_to_reorder)
{
	struct sk_buff *skb, *skb2, *skb3;

	self->data.common.sender_id = cpu_to_be64(2000);
	self->data.message_length = htonl(2000);
	skb = mock_skb_new(self->client_ip, &self->data.common, 1400, 0);
	self->data.common.sender_id = cpu_to_be64(3000);
	self->data.message_length = htonl(3000);
	skb2 = mock_skb_new(self->client_ip, &self->data.common, 1400, 0);
	self->data.common.sender_id = cpu_to_be64(5000);
	self->data.message_length = htonl(5000);
	skb3 = mock_skb_new(self->client_ip, &self->data.common, 1400, 0);
	skb_shinfo(skb)->frag_list = skb2;
	skb2->next = skb3;
	skb3->next = NULL;
	homa_softirq(skb);
	unit_log_active_ids(&self->hsk);
	EXPECT_STREQ("2001 3001 5001", unit_log_get());
}
TEST_F(homa_plumbing, homa_softirq__cant_pull_header)
{
	struct sk_buff *skb;
	skb = mock_skb_new(self->client_ip, &self->data.common, 1400, 1400);
	skb->data_len = skb->len - 20;
	homa_softirq(skb);
	EXPECT_STREQ("pskb discard", unit_log_get());
}
TEST_F(homa_plumbing, homa_softirq__remove_extra_headers)
{
	struct sk_buff *skb;
	skb = mock_skb_new(self->client_ip, &self->data.common, 1400, 1400);
	__skb_push(skb, 10);
	homa_softirq(skb);
	EXPECT_EQ(1, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_softirq__packet_too_short)
{
	struct sk_buff *skb;
	struct ack_header h;
	h.common.type = ACK;
	skb = mock_skb_new(self->client_ip, &h.common, 0, 0);
	skb->len -= 1;
	homa_softirq(skb);
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
	EXPECT_EQ(1, homa_cores[cpu_number]->metrics.short_packets);
}
TEST_F(homa_plumbing, homa_softirq__bogus_packet_type)
{
	struct sk_buff *skb;
	self->data.common.type = BOGUS;
	skb = mock_skb_new(self->client_ip, &self->data.common, 1400, 1400);
	homa_softirq(skb);
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
	EXPECT_EQ(1, homa_cores[cpu_number]->metrics.short_packets);
}
TEST_F(homa_plumbing, homa_softirq__unknown_socket_ipv4)
{
	struct sk_buff *skb;
	self->data.common.dport = htons(100);

	// Make sure the test uses IPv4.
	mock_ipv6 = false;
	homa_sock_destroy(&self->hsk);
	mock_sock_init(&self->hsk, &self->homa, 0);

	skb = mock_skb_new(self->client_ip, &self->data.common, 1400, 1400);
	homa_softirq(skb);
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
	EXPECT_STREQ("icmp_send type 3, code 3", unit_log_get());
}
TEST_F(homa_plumbing, homa_softirq__unknown_socket_ipv6)
{
	struct sk_buff *skb;
	self->data.common.dport = htons(100);

	// Make sure the test uses IPv6.
	mock_ipv6 = true;
	homa_sock_destroy(&self->hsk);
	mock_sock_init(&self->hsk, &self->homa, 0);

	skb = mock_skb_new(self->client_ip, &self->data.common, 1400, 1400);
	homa_softirq(skb);
	EXPECT_EQ(0, unit_list_length(&self->hsk.active_rpcs));
	EXPECT_STREQ("icmp6_send type 1, code 4", unit_log_get());
}
TEST_F(homa_plumbing, homa_softirq__multiple_packets_different_sockets)
{
	struct sk_buff *skb, *skb2;
	struct homa_sock sock2;
	mock_sock_init(&sock2, &self->homa, 0);
	homa_sock_bind(&self->homa.port_map, &sock2, self->server_port+1);

	skb = mock_skb_new(self->client_ip, &self->data.common, 1400, 1400);
	self->data.common.sender_id += 2;
	self->data.common.dport = htons(self->server_port+1);
	skb2 = mock_skb_new(self->client_ip, &self->data.common, 1400, 1400);
	skb_shinfo(skb)->frag_list = skb2;
	skb2->next = NULL;
	homa_softirq(skb);
	EXPECT_EQ(1, unit_list_length(&self->hsk.active_rpcs));
	EXPECT_EQ(1, unit_list_length(&sock2.active_rpcs));
	homa_sock_destroy(&sock2);
}
TEST_F(homa_plumbing, homa_softirq__multiple_packets_same_socket)
{
	struct sk_buff *skb, *skb2;
	skb = mock_skb_new(self->client_ip, &self->data.common, 1400, 1400);
	self->data.common.sender_id += cpu_to_be64(self->client_id + 2);
	skb2 = mock_skb_new(self->client_ip, &self->data.common, 1400, 1400);
	skb_shinfo(skb)->frag_list = skb2;
	skb2->next = NULL;
	homa_softirq(skb);
	EXPECT_EQ(2, unit_list_length(&self->hsk.active_rpcs));
}
TEST_F(homa_plumbing, homa_softirq__update_total_incoming)
{
	struct sk_buff *skb, *skb2;

	self->data.seg.segment_length = htonl(1400);
	skb = mock_skb_new(self->client_ip, &self->data.common, 1400, 0);
	self->data.seg.offset = htonl(1400);
	skb2 = mock_skb_new(self->client_ip, &self->data.common, 1400, 1400);
	skb_shinfo(skb2)->frag_list = skb;
	skb->next = NULL;
	homa_softirq(skb2);
	unit_log_active_ids(&self->hsk);
	EXPECT_STREQ("1235", unit_log_get());
	EXPECT_EQ(7200, atomic_read(&self->homa.total_incoming));
}

TEST_F(homa_plumbing, homa_metrics_open)
{
	EXPECT_EQ(0, homa_metrics_open(NULL, NULL));
	EXPECT_NE(NULL, self->homa.metrics);

	strcpy(self->homa.metrics, "12345");
	EXPECT_EQ(0, homa_metrics_open(NULL, NULL));
	EXPECT_EQ(5, strlen(self->homa.metrics));
	EXPECT_EQ(2, self->homa.metrics_active_opens);
}
TEST_F(homa_plumbing, homa_metrics_read__basics)
{
	char buffer[1000];
	loff_t offset = 10;
	self->homa.metrics = kmalloc(100, GFP_KERNEL);
	self->homa.metrics_capacity = 100;
	strcpy(self->homa.metrics, "0123456789abcdefghijklmnop");
	self->homa.metrics_length = 26;
	EXPECT_EQ(5, homa_metrics_read(NULL, buffer, 5, &offset));
	EXPECT_STREQ("_copy_to_user copied 5 bytes", unit_log_get());
	EXPECT_EQ(15, offset);

	unit_log_clear();
	EXPECT_EQ(11, homa_metrics_read(NULL, buffer, 1000, &offset));
	EXPECT_STREQ("_copy_to_user copied 11 bytes", unit_log_get());
	EXPECT_EQ(26, offset);

	unit_log_clear();
	EXPECT_EQ(0, homa_metrics_read(NULL, buffer, 1000, &offset));
	EXPECT_STREQ("", unit_log_get());
	EXPECT_EQ(26, offset);
}
TEST_F(homa_plumbing, homa_metrics_read__error_copying_to_user)
{
	char buffer[1000];
	loff_t offset = 10;
	self->homa.metrics = kmalloc(100, GFP_KERNEL);
	self->homa.metrics_capacity = 100;
	strcpy(self->homa.metrics, "0123456789abcdefghijklmnop");
	self->homa.metrics_length = 26;
	mock_copy_to_user_errors = 1;
	EXPECT_EQ(EFAULT, -homa_metrics_read(NULL, buffer, 5, &offset));
}

TEST_F(homa_plumbing, homa_metrics_release)
{
	self->homa.metrics_active_opens = 2;
	EXPECT_EQ(0, homa_metrics_release(NULL, NULL));
	EXPECT_EQ(1, self->homa.metrics_active_opens);

	EXPECT_EQ(0, homa_metrics_release(NULL, NULL));
	EXPECT_EQ(0, self->homa.metrics_active_opens);
}
