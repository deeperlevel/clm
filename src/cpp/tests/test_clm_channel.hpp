#pragma once

#include "test.hpp"
#include "../clm/channel.hpp"

#include <thread>

struct clm_channel_test_send_args {
	clm_id channel_id{0};
	clm_id message_id{0};
	clm_int result{CLM_CHANNEL_OPERATION_FAILURE};
};

struct clm_channel_test_receive_args {
	clm_id channel_id{0};
	clm_id message_id{0};
	bool open{false};
	clm_int result{CLM_CHANNEL_OPERATION_FAILURE};
};

static void clm_channel_test_sender(clm_channel_test_send_args* args)
{
	args->result = clm_channel_send(args->channel_id, args->message_id);
}

static void clm_channel_test_receiver(clm_channel_test_receive_args* args)
{
	args->result = clm_channel_receive(args->channel_id, &args->message_id, &args->open);
}

static void clm_channel_test_blocked_receiver(clm_channel_test_receive_args* args)
{
	args->result = clm_channel_receive(args->channel_id, &args->message_id, &args->open);
}

static int test_clm_channel()
{
	clm_test_result result = {0};
	clm_test_begin("CLM - Channel");
	const clm_id buffered_id = 93001;
	const clm_id rendezvous_id = 93002;
	const clm_id close_waiter_id = 93003;
	clm_id message_id = 999;
	bool open = false;
	clm_channel_info info = {0, 0, false};

	CLM_ASSERT(clm_channel_create(buffered_id, 2) ==
		CLM_CHANNEL_OPERATION_SUCCESS &&
		clm_channel_try_send(buffered_id, 10) == CLM_CHANNEL_OPERATION_SUCCESS &&
		clm_channel_try_send(buffered_id, 0) == CLM_CHANNEL_OPERATION_SUCCESS &&
		clm_channel_try_send(buffered_id, 30) == CLM_CHANNEL_OPERATION_FULL &&
		clm_channel_get_info(buffered_id, &info) ==
		CLM_CHANNEL_OPERATION_SUCCESS && info.capacity == 2 && info.count == 2 &&
		!info.closed,
		"buffer values FIFO and report full capacity");
	CLM_ASSERT(clm_channel_send_timeout(buffered_id, 30, 2) ==
		CLM_CHANNEL_OPERATION_TIMEOUT,
		"time out a blocking send while the buffer is full");
	CLM_ASSERT(clm_channel_try_receive(buffered_id, &message_id, &open) ==
		CLM_CHANNEL_OPERATION_SUCCESS && message_id == 10 && open &&
		clm_channel_try_receive(buffered_id, &message_id, &open) ==
		CLM_CHANNEL_OPERATION_SUCCESS && message_id == 0 && open &&
		clm_channel_try_receive(buffered_id, &message_id, &open) ==
		CLM_CHANNEL_OPERATION_EMPTY,
		"receive buffered values including a valid zero ID");
	CLM_ASSERT(clm_channel_receive_timeout(buffered_id, &message_id, &open, 2) ==
		CLM_CHANNEL_OPERATION_TIMEOUT,
		"time out a blocking receive while the channel is empty");
	CLM_ASSERT(clm_channel_close(buffered_id) == CLM_CHANNEL_OPERATION_SUCCESS &&
		clm_channel_close(buffered_id) == CLM_CHANNEL_OPERATION_CLOSED &&
		clm_channel_try_send(buffered_id, 40) == CLM_CHANNEL_OPERATION_CLOSED &&
		clm_channel_try_receive(buffered_id, &message_id, &open) ==
		CLM_CHANNEL_OPERATION_CLOSED && message_id == 0 && !open &&
		clm_channel_destroy(buffered_id) == CLM_CHANNEL_OPERATION_SUCCESS,
		"close buffered channel and distinguish closed from message ID zero");

	CLM_ASSERT(clm_channel_create(rendezvous_id, 0) ==
		CLM_CHANNEL_OPERATION_SUCCESS &&
		clm_channel_try_send(rendezvous_id, 51) == CLM_CHANNEL_OPERATION_FULL &&
		clm_channel_try_receive(rendezvous_id, &message_id, &open) ==
		CLM_CHANNEL_OPERATION_EMPTY,
		"create an unbuffered rendezvous channel");
	clm_channel_test_send_args send_args = {rendezvous_id, 52,
		CLM_CHANNEL_OPERATION_FAILURE};
	std::thread sender_thread(clm_channel_test_sender, &send_args);
	CLM_ASSERT(clm_channel_receive(rendezvous_id, &message_id, &open) ==
		CLM_CHANNEL_OPERATION_SUCCESS && message_id == 52 && open,
		"receive unbuffered handoff message");
	if (sender_thread.joinable())
		sender_thread.join();
	CLM_ASSERT(send_args.result == CLM_CHANNEL_OPERATION_SUCCESS,
		"pair a blocking unbuffered sender with a receiver");

	clm_channel_test_receive_args receive_args = {rendezvous_id, 999, false,
		CLM_CHANNEL_OPERATION_FAILURE};
	std::thread receiver_thread(clm_channel_test_receiver, &receive_args);
	// give the receiver thread time to enter wait
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	CLM_ASSERT(clm_channel_send(rendezvous_id, 0) == CLM_CHANNEL_OPERATION_SUCCESS,
		"send zero ID to waiting receiver");
	if (receiver_thread.joinable())
		receiver_thread.join();
	CLM_ASSERT(receive_args.result == CLM_CHANNEL_OPERATION_SUCCESS &&
		receive_args.message_id == 0 && receive_args.open,
		"pair a receiver with a blocking unbuffered sender");
	CLM_ASSERT(clm_channel_close(rendezvous_id) == CLM_CHANNEL_OPERATION_SUCCESS &&
		clm_channel_receive(rendezvous_id, &message_id, &open) ==
		CLM_CHANNEL_OPERATION_CLOSED && message_id == 0 && !open &&
		clm_channel_destroy(rendezvous_id) == CLM_CHANNEL_OPERATION_SUCCESS,
		"close and destroy a drained rendezvous channel");

	CLM_ASSERT(clm_channel_create(close_waiter_id, 1) ==
		CLM_CHANNEL_OPERATION_SUCCESS,
		"create a channel for a blocked receiver");
	receive_args.channel_id = close_waiter_id;
	receive_args.message_id = 777;
	receive_args.open = true;
	receive_args.result = CLM_CHANNEL_OPERATION_FAILURE;
	std::thread blocked_receiver_thread(clm_channel_test_blocked_receiver, &receive_args);
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	CLM_ASSERT(clm_channel_close(close_waiter_id) == CLM_CHANNEL_OPERATION_SUCCESS,
		"close channel while receiver is blocked");
	if (blocked_receiver_thread.joinable())
		blocked_receiver_thread.join();
	CLM_ASSERT(receive_args.result == CLM_CHANNEL_OPERATION_CLOSED &&
		receive_args.message_id == 0 && !receive_args.open &&
		clm_channel_send_timeout(close_waiter_id, 1, 0) ==
		CLM_CHANNEL_OPERATION_CLOSED &&
		clm_channel_destroy(close_waiter_id) == CLM_CHANNEL_OPERATION_SUCCESS,
		"return closed to a receiver after close and reject later sends");

	clm_test_summary(result.passed, result.failed);
	return result.failed == 0 ? 0 : 1;
}

