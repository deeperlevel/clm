"""Tests for procedural CLM Dispatch and its convenience facade."""
import asyncio
import unittest

from clm_container import clm_container_get_object, CONTAINER_SUCCESS
from clm_depend import (
    clm_depend_initialize, clm_depend_destroy, clm_depend_register,
    DependLifetime,
)
from clm_dispatch import (
    Chain, Dispatch,
    DispatchError,
    HANDLER_CONTAINER,
    COMMAND_LOGGER,
    EVENT_LOGGER,
    QUERY_LOGGER,
    EXCEPTION_LOGGER,
    clm_dispatch_destroy,
    clm_dispatch_execute,
    clm_dispatch_emit,
    clm_dispatch_query,
    clm_dispatch_caught,
    clm_dispatch_register_handler,
    clm_dispatch_register_logger,
    clm_dispatch_start_new,
    clm_dispatch_next
)


async def async_echo(message):
    return message


# noinspection unused-parameter
async def async_none(message):
    return None

async def async_double(message):
    return message * 2

async def async_increment(message):
    return message + 1


# noinspection unused-parameter
async def async_ok(message):
    return "ok"


# noinspection unused-parameter
async def async_inner(message):
    return "inner"


# noinspection unused-parameter
async def async_invoked(message):
    return "invoked"


# noinspection bad-argument-type,unused-parameter,redundant-parentheses
class DispatchTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        clm_dispatch_destroy()
        clm_depend_destroy()
        clm_depend_initialize()

    async def asyncTearDown(self):
        clm_dispatch_destroy()
        clm_depend_destroy()

    async def test_async_handler_and_dependencies(self):
        clm_depend_register(7, [], lambda args: "resolved", DependLifetime.LAZY)
        async def handler(dependency, value):
            return dependency, value
        clm_dispatch_register_handler(1, [7], handler)
        self.assertEqual(await clm_dispatch_execute(1, "payload"), ("resolved", "payload"))

    async def test_async_handler_and_facade(self):
        Dispatch.register_handler(2, [], async_double)
        self.assertEqual(await Dispatch.query(2, 3), 6)

    async def test_all_categories_and_logging(self):
        events = []
        for logger_id in (COMMAND_LOGGER, EVENT_LOGGER, QUERY_LOGGER, EXCEPTION_LOGGER):
            clm_dispatch_register_logger(logger_id, lambda envelope: events.append(envelope))
        clm_dispatch_register_handler(3, [], async_echo)
        for dispatch, name in ((clm_dispatch_execute, "command"),
                               (clm_dispatch_emit, "event"),
                               (clm_dispatch_query, "query"),
                               (clm_dispatch_caught, "exception")):
            self.assertEqual(await dispatch(3, name), name)
        self.assertEqual([e.context for e in events], ["command", "event", "query", "exception"])
        self.assertTrue(all(isinstance(e.chain, Chain) for e in events))

    async def test_log_false(self):
        seen = []
        clm_dispatch_register_logger(COMMAND_LOGGER, lambda envelope: seen.append(envelope))
        clm_dispatch_register_handler(4, [], async_echo)
        self.assertEqual(await clm_dispatch_execute(4, "data", log=False), "data")
        self.assertEqual(seen, [])

    async def test_no_logger_is_acceptable(self):
        clm_dispatch_register_handler(5, [], async_ok)
        self.assertEqual(await clm_dispatch_execute(5, 0), "ok")

    async def test_missing_handler(self):
        with self.assertRaises(DispatchError):
            await clm_dispatch_execute(999, None)

    async def test_duplicate_handler_and_logger(self):
        clm_dispatch_register_handler(6, [], async_none)
        with self.assertRaises(DispatchError):
            clm_dispatch_register_handler(6, [], async_none)
        clm_dispatch_register_logger(COMMAND_LOGGER, lambda envelope: None)
        with self.assertRaises(DispatchError):
            clm_dispatch_register_logger(COMMAND_LOGGER, lambda envelope: None)

    async def test_chain_methods(self):
        root = clm_dispatch_start_new()
        child = clm_dispatch_next(root)
        self.assertNotEqual(root.id, child.id)
        self.assertEqual(root.id, child.correlation_id)
        self.assertEqual(root.id, child.causation_id)

    async def test_nested_trace_context(self):
        captured = []
        clm_dispatch_register_logger(COMMAND_LOGGER, lambda envelope: captured.append(envelope.chain))
        clm_dispatch_register_handler(10, [], async_inner)
        async def outer(msg):
            return await clm_dispatch_execute(10, "nested")
        clm_dispatch_register_handler(11, [], outer)
        self.assertEqual(await clm_dispatch_execute(11, "outer"), "inner")
        self.assertEqual(captured[0].correlation_id, captured[1].correlation_id)
        self.assertEqual(captured[1].causation_id, captured[0].id)

    async def test_explicit_trace(self):
        seen = []
        clm_dispatch_register_logger(EVENT_LOGGER, lambda envelope: seen.append(envelope.chain))
        clm_dispatch_register_handler(12, [], async_none)
        root = clm_dispatch_start_new()
        await clm_dispatch_emit(12, {}, chain=root)
        self.assertEqual(seen[0].causation_id, root.id)

    async def test_concurrent_trace_isolation(self):
        seen = []
        async def logger(envelope):
            await asyncio.sleep(0)
            seen.append(envelope.chain)
        clm_dispatch_register_logger(COMMAND_LOGGER, logger)
        async def handler(msg):
            await asyncio.sleep(0)
            return msg
        clm_dispatch_register_handler(13, [], handler)
        self.assertEqual(await asyncio.gather(*(
            clm_dispatch_execute(13, i) for i in range(10)
        )), list(range(10)))
        self.assertEqual(len({chain.correlation_id for chain in seen}), 10)

    async def test_cleanup_and_reinitialize(self):
        clm_dispatch_register_handler(14, [], async_echo)
        clm_dispatch_destroy()
        clm_dispatch_register_handler(14, [], async_increment)
        self.assertEqual(await clm_dispatch_query(14, 4), 5)

    async def test_invalid_registration(self):
        with self.assertRaises(ValueError):
            clm_dispatch_register_handler(-1, [], async_none)
        with self.assertRaises(TypeError):
            clm_dispatch_register_handler(1, [], None)
        with self.assertRaises(ValueError):
            clm_dispatch_register_logger(1, lambda msg: None)

    async def test_sync_logger_failure_is_ignored(self):
        def logger(msg):
            raise ValueError("bad logger")
        clm_dispatch_register_logger(COMMAND_LOGGER, logger)
        clm_dispatch_register_handler(20, [], async_invoked)
        self.assertEqual(await clm_dispatch_execute(20, "message"), "invoked")

    async def test_async_logger_failure_is_ignored(self):
        async def logger(msg):
            raise RuntimeError("async logger failed")
        clm_dispatch_register_logger(EVENT_LOGGER, logger)
        clm_dispatch_register_handler(22, [], async_echo)
        self.assertEqual(await clm_dispatch_emit(22, "event"), "event")

    async def test_handler_failure_still_propagates(self):
        clm_dispatch_register_logger(COMMAND_LOGGER, lambda msg: None)
        async def handler(msg):
            raise RuntimeError("handler failed")
        clm_dispatch_register_handler(23, [], handler)
        with self.assertRaisesRegex(RuntimeError, "handler failed"):
            await clm_dispatch_execute(23, "message")

    async def test_sync_handler_registration_is_rejected(self):
        def sync_handler(message):
            return message
        with self.assertRaises(TypeError):
            clm_dispatch_register_handler(30, [], sync_handler)

    async def test_multiple_dependencies_are_positional(self):
        clm_depend_register(101, [], lambda args: "repo", DependLifetime.LAZY)
        clm_depend_register(102, [], lambda args: "logger", DependLifetime.LAZY)
        async def handler(repo, logger, message):
            return (repo, logger, message)
        clm_dispatch_register_handler(31, [101, 102], handler)
        self.assertEqual(await clm_dispatch_execute(31, "payload"), ("repo", "logger", "payload"))

    async def test_async_handler_return_value(self):
        async def handler(message):
            await asyncio.sleep(0)
            return {"result": message}
        clm_dispatch_register_handler(32, [], handler)
        self.assertEqual(await Dispatch.query(32, 123), {"result": 123})

    async def test_container_storage(self):
        clm_dispatch_register_handler(21, [], async_none)
        code, value = clm_container_get_object(HANDLER_CONTAINER, 21)
        self.assertEqual(code, CONTAINER_SUCCESS)
        self.assertEqual(value.dependencies, ())


if __name__ == "__main__":
    unittest.main()
