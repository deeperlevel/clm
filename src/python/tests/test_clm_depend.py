import unittest
from clm_depend import (
    Depend,
    DependStatus,
    DependLifetime,
    DependError,
    clm_depend_construct
)


class DependTests(unittest.TestCase):
    def setUp(self):
        Depend.destroy()
        Depend.initialize()

    def tearDown(self):
        Depend.destroy()

    def test_eager_and_singleton_alias(self):
        created = []
        self.assertEqual(DependLifetime.EAGER, DependLifetime.SINGLETON)
        self.assertEqual(Depend.register(1, [], lambda args: created.append(1) or object()), DependStatus.SUCCESS)
        self.assertEqual(created, [1])
        self.assertIs(Depend.resolve(1), Depend.resolve(1))

    def test_lazy_and_none_singleton(self):
        created = []
        self.assertEqual(Depend.register(2, [], lambda _: created.append(1) or None, DependLifetime.LAZY), DependStatus.SUCCESS)
        self.assertEqual(created, [])
        self.assertIsNone(Depend.resolve(2))
        self.assertIsNone(Depend.resolve(2))
        self.assertEqual(created, [1])

    def test_transient_with_dependencies_and_invoke(self):
        self.assertEqual(Depend.register(1, [], lambda _: {'version': 7}), DependStatus.SUCCESS)
        self.assertEqual(Depend.register(2, [1], lambda items: {'source': items[0]}, DependLifetime.TRANSIENT), DependStatus.SUCCESS)
        self.assertIsNot(Depend.resolve(2), Depend.resolve(2))
        self.assertIs(Depend.resolve(2)['source'], Depend.resolve(1))
        self.assertEqual(Depend.invoke([1], lambda x: x['version']), 7)

    def test_missing_deps_and_circular(self):
        self.assertEqual(Depend.register(1, [2], lambda _: None), DependStatus.MISSING_DEPENDENCY)
        self.assertEqual(Depend.register(1, [2], lambda _: None, DependLifetime.LAZY), DependStatus.SUCCESS)
        self.assertEqual(Depend.register(2, [1], lambda _: None, DependLifetime.LAZY), DependStatus.SUCCESS)
        with self.assertRaises(DependError) as ctx:
            Depend.resolve(1)
        self.assertEqual(ctx.exception.status, DependStatus.CIRCULAR_DEPENDENCY)

    def test_scoped(self):
        self.assertEqual(Depend.register(1, [], lambda _: object(), DependLifetime.SCOPED), DependStatus.SUCCESS)
        with self.assertRaises(DependError) as ctx:
            Depend.resolve(1)
        self.assertEqual(ctx.exception.status, DependStatus.SCOPE_REQUIRED)
        Depend.begin_scope()
        first = Depend.resolve(1)
        self.assertIs(first, Depend.resolve(1))
        with self.assertRaises(DependError):
            Depend.begin_scope()
        Depend.end_scope()
        Depend.begin_scope()
        self.assertIsNot(first, Depend.resolve(1))
        Depend.end_scope()

    def test_incompatible_lifetime(self):
        Depend.register(1, [], lambda _: object(), DependLifetime.SCOPED)
        self.assertEqual(Depend.register(2, [1], lambda a: a[0]), DependStatus.INCOMPATIBLE_LIFETIME)
        Depend.register(3, [1], lambda a: a[0], DependLifetime.LAZY)
        Depend.begin_scope()
        with self.assertRaises(DependError) as ctx:
            Depend.resolve(3)
        self.assertEqual(ctx.exception.status, DependStatus.INCOMPATIBLE_LIFETIME)
        Depend.end_scope()

    def test_construct_and_seal(self):
        Depend.register(1, [], lambda _: 1, DependLifetime.LAZY)
        clm_depend_construct(1, lambda _: 2)
        self.assertEqual(Depend.resolve(1), 2)
        with self.assertRaises(DependError) as ctx:
            Depend.construct(1, lambda _: 3)
        self.assertEqual(ctx.exception.status, DependStatus.CONSTRUCTOR_LOCKED)
        Depend.seal()
        self.assertEqual(Depend.register(4, [], lambda _: 4), DependStatus.SEALED)
        with self.assertRaises(DependError):
            Depend.unregister(1)

    def test_unregister_and_invalid(self):
        self.assertEqual(Depend.register(-1, [], lambda _: 1), DependStatus.INVALID_ARGUMENT)
        self.assertEqual(Depend.register(1, [], lambda _: 1, DependLifetime.LAZY), DependStatus.SUCCESS)
        self.assertEqual(Depend.register(1, [], lambda _: 2), DependStatus.ALREADY_REGISTERED)
        Depend.unregister(1)
        with self.assertRaises(DependError):
            Depend.resolve(1)
        self.assertEqual(Depend.register(1, [], lambda _: 3), DependStatus.SUCCESS)
        self.assertEqual(Depend.resolve(1), 3)

    def test_registration_failure_rolls_back(self):
        def explode(_):
            raise ValueError('factory failure')
        with self.assertRaises(ValueError):
            Depend.register(1, [], explode)
        self.assertEqual(Depend.register(1, [], lambda _: 1), DependStatus.SUCCESS)


if __name__ == '__main__':
    unittest.main()
