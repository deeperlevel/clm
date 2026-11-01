#pragma once

#include "test.h"
#include "../clm/channel.h"

#include "../clm/thread.h"

typedef struct clm_channel_test_send_args {
	clm_id channel_id;
	clm_id message_id;
	clm_int result;
} clm_channel_test_send_args;

typedef struct clm_channel_test_receive_args {
	clm_id channel_id;
	clm_id message_id;
	bool open;
	clm_int result;
} clm_channel_test_receive_args;

static int clm_channel_test_sender(void* context)
{
	clm_channel_test_send_args* args =
		(clm_channel_test_send_args*)context;
	args->result = clm_channel_send(args->channel_id, args->message_id);
	return 0;
}

static int clm_channel_test_receiver(void* context)
{
	clm_channel_test_receive_args* args =
		(clm_channel_test_receive_args*)context;
	args->result = clm_channel_receive(args->channel_id, &args->message_id,
		&args->open);
	return 0;
}

static int clm_channel_test_blocked_receiver(void* context)
{
	clm_channel_test_receive_args* args =
		(clm_channel_test_receive_args*)context;
	args->result = clm_channel_receive(args->channel_id, &args->message_id,
		&args->open);
	return 0;
}

static int clm_channel_tests(void)
{
	clm_test_result result = {0};
	clm_test_begin("CLM - Channel");
	const clm_id buffered_id = 93001;
	const clm_id rendezvous_id = 93002;
	const clm_id close_waiter_id = 93003;
	clm_id message_id = 999;
	bool open = false;
	clm_channel_info info = {0};

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
	thrd_t sender;
	CLM_ASSERT(thrd_create(&sender, clm_channel_test_sender, &send_args) ==
		thrd_success &&
		clm_channel_receive(rendezvous_id, &message_id, &open) ==
		CLM_CHANNEL_OPERATION_SUCCESS && message_id == 52 && open &&
		thrd_join(sender, NULL) == thrd_success &&
		send_args.result == CLM_CHANNEL_OPERATION_SUCCESS,
		"pair a blocking unbuffered sender with a receiver");
	clm_channel_test_receive_args receive_args = {rendezvous_id, 999, false,
		CLM_CHANNEL_OPERATION_FAILURE};
	thrd_t receiver;
	CLM_ASSERT(thrd_create(&receiver, clm_channel_test_receiver, &receive_args) ==
		thrd_success && clm_channel_send(rendezvous_id, 0) ==
		CLM_CHANNEL_OPERATION_SUCCESS && thrd_join(receiver, NULL) == thrd_success &&
		receive_args.result == CLM_CHANNEL_OPERATION_SUCCESS &&
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
	CLM_ASSERT(thrd_create(&receiver, clm_channel_test_blocked_receiver,
		&receive_args) == thrd_success &&
		clm_channel_close(close_waiter_id) == CLM_CHANNEL_OPERATION_SUCCESS &&
		thrd_join(receiver, NULL) == thrd_success &&
		receive_args.result == CLM_CHANNEL_OPERATION_CLOSED &&
		receive_args.message_id == 0 && !receive_args.open &&
		clm_channel_send_timeout(close_waiter_id, 1, 0) ==
		CLM_CHANNEL_OPERATION_CLOSED &&
		clm_channel_destroy(close_waiter_id) == CLM_CHANNEL_OPERATION_SUCCESS,
		"return closed to a receiver after close and reject later sends");

	clm_test_summary(result.passed, result.failed);
	return result.failed == 0 ? 0 : 1;
}