import unittest
from clm_depend import (clm_depend_initialize, clm_depend_destroy, clm_depend_get_info,
                        DependError, DependStatus, clm_depend_resolve)
from clm_ecs import *

POS, HP, SHIP, ASTEROID, SYSTEM = 0x3001, 0x3002, 0x3003, 0x3004, 0x3005


class ECSTests(unittest.TestCase):
    def setUp(self):
        clm_ecs_reset()
        clm_depend_initialize()

    def tearDown(self):
        clm_depend_destroy()
        clm_ecs_reset()

    def register(self):
        self.assertEqual(clm_ecs_register_component(POS, lambda deps: {'x': 0}), ECSStatus.SUCCESS)
        self.assertEqual(clm_ecs_register_component(HP, lambda deps: {'hp': 100}), ECSStatus.SUCCESS)
        self.assertEqual(clm_ecs_register_entity(SHIP, [POS, HP],
                         lambda deps: {'position': deps[0], 'health': deps[1]}), ECSStatus.SUCCESS)
        self.assertEqual(clm_ecs_register_entity(ASTEROID, [POS], lambda deps: tuple(deps)), ECSStatus.SUCCESS)

    def test_registration_and_validation(self):
        self.register()
        self.assertEqual(clm_ecs_register_component(POS, lambda d: None), ECSStatus.ALREADY_REGISTERED)
        self.assertEqual(clm_ecs_register_entity(8000, [9999], lambda d: None), ECSStatus.NOT_COMPONENT)
        self.assertEqual(clm_ecs_register_entity(8001, [POS, POS], lambda d: None), ECSStatus.INVALID_ARGUMENT)
        with self.assertRaises(ECSError):
            clm_ecs_create_entity(POS)

    def test_distinct_transients_and_component_ids(self):
        self.register()
        a = clm_ecs_create_entity(SHIP)
        b = clm_ecs_create_entity(SHIP)
        self.assertNotEqual(a, b)
        ca, cb = clm_ecs_get_components(a), clm_ecs_get_components(b)
        self.assertNotEqual(ca[POS], cb[POS])
        self.assertEqual(clm_ecs_get_entity(a)['position'], ca[POS])
        self.assertEqual(clm_ecs_get_component(ca[HP]), {'hp': 100})
        self.assertEqual(clm_depend_get_info(ca[POS]).references, 1)

    def test_disposal_releases_component_tree(self):
        self.register()
        a = clm_ecs_create_entity(SHIP)
        position = clm_ecs_get_components(a)[POS]
        clm_ecs_unreference(a)
        self.assertEqual(clm_ecs_dispose(), 3)
        with self.assertRaises(DependError):
            clm_depend_get_info(position)
        self.assertEqual(clm_ecs_query([POS]), [])

    def test_extra_owner_preserves_component(self):
        self.register()
        a = clm_ecs_create_entity(SHIP)
        position = clm_ecs_get_components(a)[POS]
        clm_ecs_reference(position)
        clm_ecs_unreference(a)
        self.assertEqual(clm_ecs_dispose(), 2)
        self.assertEqual(clm_depend_get_info(position).references, 1)
        clm_ecs_unreference(position)
        self.assertEqual(clm_ecs_dispose(), 1)

    def test_query_and_run_system(self):
        self.register()
        first = clm_ecs_create_entity(SHIP)
        clm_ecs_create_entity(ASTEROID)
        clm_ecs_create_entity(SHIP)
        seen = []
        self.assertEqual(clm_ecs_register_system(SYSTEM, [POS, HP],
                           lambda iid, mapping: seen.append((iid, clm_ecs_get_component(mapping[HP])['hp']))), ECSStatus.SUCCESS)
        self.assertEqual(clm_ecs_get_system_info(SYSTEM).components, (POS, HP))
        self.assertEqual(clm_ecs_run_system(SYSTEM), 2)
        self.assertEqual(len(seen), 2)
        clm_ecs_unreference(first)
        self.assertEqual(len(clm_ecs_query([POS, HP])), 1)

    def test_external_dispose_prunes_stale_instances(self):
        self.register()
        a = clm_ecs_create_entity(SHIP)
        clm_ecs_unreference(a)
        from clm_depend import clm_depend_dispose
        clm_depend_dispose()
        self.assertEqual(clm_ecs_query([POS]), [])

    def test_reset_does_not_destroy_depend(self):
        self.register()
        a = clm_ecs_create_entity(SHIP)
        clm_ecs_reset()
        self.assertEqual(clm_depend_get_info(a).service_id, SHIP)


if __name__ == '__main__':
    unittest.main()
