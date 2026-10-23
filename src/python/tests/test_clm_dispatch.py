"""Tests for procedural CLM Dispatch and its convenience facade."""
import asyncio
import unittest

from clm_container import clm_container_get_object, CONTAINER_SUCCESS
from clm_depend import (
    clm_depend_initialize, clm_depend_destroy, clm_depend_register,
    DependLifetime,
)
from clm_dispatch import (
    Chain, Dispatch, DispatchError, HANDLER_CONTAINER,
    COMMAND_LOGGER, EVENT_LOGGER, QUERY_LOGGER, EXCEPTION_LOGGER,
    clm_dispatch_destroy, clm_dispatch_execute, clm_dispatch_emit,
    clm_dispatch_query, clm_dispatch_caught, clm_dispatch_register_handler,
    clm_dispatch_register_logger, clm_dispatch_start_new, clm_dispatch_next,
)


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
        async def handler(deps, value):
            return deps[0], value
        clm_dispatch_register_handler(1, [7], handler)
        self.assertEqual(await clm_dispatch_execute(1, "payload"), ("resolved", "payload"))

    async def test_sync_handler_and_facade(self):
        Dispatch.register_handler(2, [], lambda deps, msg: msg * 2)
        self.assertEqual(await Dispatch.query(2, 3), 6)

    async def test_all_categories_and_logging(self):
        events = []
        for logger_id in (COMMAND_LOGGER, EVENT_LOGGER, QUERY_LOGGER, EXCEPTION_LOGGER):
            clm_dispatch_register_logger(logger_id, lambda envelope: events.append(envelope))
        clm_dispatch_register_handler(3, [], lambda deps, payload: payload)
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
        clm_dispatch_register_handler(4, [], lambda deps, msg: msg)
        self.assertEqual(await clm_dispatch_execute(4, "data", log=False), "data")
        self.assertEqual(seen, [])

    async def test_no_logger_is_acceptable(self):
        clm_dispatch_register_handler(5, [], lambda deps, msg: "ok")
        self.assertEqual(await clm_dispatch_execute(5, 0), "ok")

    async def test_missing_handler(self):
        with self.assertRaises(DispatchError):
            await clm_dispatch_execute(999, None)

    async def test_duplicate_handler_and_logger(self):
        clm_dispatch_register_handler(6, [], lambda deps, msg: None)
        with self.assertRaises(DispatchError):
            clm_dispatch_register_handler(6, [], lambda deps, msg: None)
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
        clm_dispatch_register_handler(10, [], lambda deps, msg: "inner")
        async def outer(deps, msg):
            return await clm_dispatch_execute(10, "nested")
        clm_dispatch_register_handler(11, [], outer)
        self.assertEqual(await clm_dispatch_execute(11, "outer"), "inner")
        self.assertEqual(captured[0].correlation_id, captured[1].correlation_id)
        self.assertEqual(captured[1].causation_id, captured[0].id)

    async def test_explicit_trace(self):
        seen = []
        clm_dispatch_register_logger(EVENT_LOGGER, lambda envelope: seen.append(envelope.chain))
        clm_dispatch_register_handler(12, [], lambda deps, msg: None)
        root = clm_dispatch_start_new()
        await clm_dispatch_emit(12, {}, chain=root)
        self.assertEqual(seen[0].causation_id, root.id)

    async def test_concurrent_trace_isolation(self):
        seen = []
        async def logger(envelope):
            await asyncio.sleep(0)
            seen.append(envelope.chain)
        clm_dispatch_register_logger(COMMAND_LOGGER, logger)
        async def handler(deps, msg):
            await asyncio.sleep(0)
            return msg
        clm_dispatch_register_handler(13, [], handler)
        self.assertEqual(await asyncio.gather(*(
            clm_dispatch_execute(13, i) for i in range(10)
        )), list(range(10)))
        self.assertEqual(len({chain.correlation_id for chain in seen}), 10)

    async def test_cleanup_and_reinitialize(self):
        clm_dispatch_register_handler(14, [], lambda deps, msg: msg)
        clm_dispatch_destroy()
        clm_dispatch_register_handler(14, [], lambda deps, msg: msg + 1)
        self.assertEqual(await clm_dispatch_query(14, 4), 5)

    async def test_invalid_registration(self):
        with self.assertRaises(ValueError):
            clm_dispatch_register_handler(-1, [], lambda deps, msg: None)
        with self.assertRaises(TypeError):
            clm_dispatch_register_handler(1, [], None)
        with self.assertRaises(ValueError):
            clm_dispatch_register_logger(1, lambda msg: None)

    async def test_sync_logger_failure_is_ignored(self):
        def logger(msg):
            raise ValueError("bad logger")
        clm_dispatch_register_logger(COMMAND_LOGGER, logger)
        clm_dispatch_register_handler(20, [], lambda deps, msg: "invoked")
        self.assertEqual(await clm_dispatch_execute(20, "message"), "invoked")

    async def test_async_logger_failure_is_ignored(self):
        async def logger(msg):
            raise RuntimeError("async logger failed")
        clm_dispatch_register_logger(EVENT_LOGGER, logger)
        clm_dispatch_register_handler(22, [], lambda deps, msg: msg)
        self.assertEqual(await clm_dispatch_emit(22, "event"), "event")

    async def test_handler_failure_still_propagates(self):
        clm_dispatch_register_logger(COMMAND_LOGGER, lambda msg: None)
        def handler(deps, msg):
            raise RuntimeError("handler failed")
        clm_dispatch_register_handler(23, [], handler)
        with self.assertRaisesRegex(RuntimeError, "handler failed"):
            await clm_dispatch_execute(23, "message")

    async def test_container_storage(self):
        clm_dispatch_register_handler(21, [], lambda deps, msg: None)
        code, value = clm_container_get_object(HANDLER_CONTAINER, 21)
        self.assertEqual(code, CONTAINER_SUCCESS)
        self.assertEqual(value.dependencies, ())


if __name__ == "__main__":
    unittest.main()
