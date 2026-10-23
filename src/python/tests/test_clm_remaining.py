"""Integration contracts for CLM's extended ECS, Storage, Managed and Dispatch."""
import asyncio
import unittest
from clm_depend import (clm_depend_initialize, clm_depend_destroy, clm_depend_register,
    clm_depend_resolve, clm_depend_unreference, clm_depend_get_info, clm_depend_get,
    clm_depend_dispose, DependLifetime, DependError)
from clm_ecs import (clm_ecs_reset, clm_ecs_register_component, clm_ecs_register_entity,
    clm_ecs_create_entity, clm_ecs_attach_component, clm_ecs_remove_component,
    clm_ecs_get_components, clm_ecs_query, clm_ecs_unreference, ECSStatus)
from clm_managed import *
from clm_dispatch import (clm_dispatch_destroy, clm_dispatch_register_handler, clm_dispatch_execute)
from clm_storage import *

class RemainingTests(unittest.TestCase):
    def setUp(self):
        clm_dispatch_destroy(); clm_ecs_reset(); clm_managed_destroy()
        clm_depend_destroy(); clm_depend_initialize(); clm_managed_initialize()

    def tearDown(self):
        clm_dispatch_destroy(); clm_ecs_reset(); clm_managed_destroy(); clm_depend_destroy()
        for sid in (901,902,903,904):
            clm_storage_destroy(sid)

    def test_dynamic_entity_components_and_dependency_ownership(self):
        self.assertEqual(clm_ecs_register_component(1111, lambda deps: {'x':0}), ECSStatus.SUCCESS)
        self.assertEqual(clm_ecs_register_component(1112, lambda deps: {'hp':100}), ECSStatus.SUCCESS)
        self.assertEqual(clm_ecs_register_entity(1113,[1111],lambda deps:{'position_id':deps[0]}),ECSStatus.SUCCESS)
        eid = clm_ecs_create_entity(1113)
        pid = clm_ecs_get_components(eid)[1111]
        hid = clm_ecs_attach_component(eid,1112)
        self.assertEqual(clm_ecs_get_components(eid)[1112],hid)
        self.assertEqual(clm_ecs_query([1111,1112])[0][0],eid)
        with self.assertRaises(Exception): clm_ecs_attach_component(eid,1112)
        self.assertEqual(clm_ecs_remove_component(eid,1112), hid)
        self.assertEqual(clm_depend_get_info(hid).references,0)
        self.assertEqual(clm_depend_dispose(),1)
        self.assertNotIn(1112,clm_ecs_get_components(eid))
        clm_ecs_unreference(eid)
        self.assertEqual(clm_depend_dispose(),2)
        with self.assertRaises(DependError): clm_depend_get_info(pid)

    def test_threshold_and_hybrid_collection(self):
        self.assertEqual(clm_managed_set_algorithm(ManagedAlgorithm.THRESHOLD), ManagedStatus.SUCCESS)
        self.assertEqual(clm_managed_set_threshold(2), ManagedStatus.SUCCESS)
        self.assertEqual(clm_managed_enable(), ManagedStatus.SUCCESS)
        clm_storage_create(901, STORAGE_TYPE_INTEGER)
        clm_storage_unreference(901)
        self.assertFalse(clm_managed_poll())
        clm_storage_create(902, STORAGE_TYPE_INTEGER)
        clm_storage_unreference(902)
        self.assertTrue(clm_managed_poll())
        self.assertNotEqual(clm_storage_get_info(901)[0],STORAGE_SUCCESS)
        clm_managed_disable()
        self.assertEqual(clm_managed_set_algorithm(ManagedAlgorithm.HYBRID), ManagedStatus.SUCCESS)
        self.assertEqual(clm_managed_set_threshold(1), ManagedStatus.SUCCESS)
        clm_managed_enable()
        clm_storage_create(903, STORAGE_TYPE_INTEGER)
        clm_storage_unreference(903)
        self.assertTrue(clm_managed_poll())

    def test_storage_navigation_and_reverse_atlas_plane(self):
        self.assertEqual(clm_storage_atlas3d_create(901,2,2,2,1,1,2,storage_type=STORAGE_TYPE_INTEGER),STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas_create(902,2,2,1,1,storage_type=STORAGE_TYPE_INTEGER),STORAGE_SUCCESS)
        clm_storage_grid_set(902,1,0,42)
        self.assertEqual(clm_storage_atlas3d_set_slice(902,901,layer=1,depth=1),STORAGE_SUCCESS)
        self.assertEqual(clm_storage_grid_get(901,1,0,z=1,layer=1),(STORAGE_SUCCESS,42))
        self.assertEqual(clm_storage_atlas3d_set_position(901,1,0,1,0,0,1),STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas3d_get_position(901),(STORAGE_SUCCESS,(1,0,1,0,0,1)))
        self.assertEqual(clm_storage_grid_get_current(901),(STORAGE_SUCCESS,42))
        self.assertEqual(clm_storage_atlas3d_move_first(901),STORAGE_SUCCESS)
        self.assertEqual(clm_storage_atlas3d_get_position(901),(STORAGE_SUCCESS,(0,0,0,0,0,0)))

    def test_dispatch_releases_transient_after_success_and_failure(self):
        clm_depend_register(2101, [],lambda deps:{'name':'ship'},DependLifetime.TRANSIENT)
        async def good(deps, payload):
            self.assertEqual(deps[0]['name'],'ship')
            return payload
        clm_dispatch_register_handler(2102,[2101],good)
        self.assertEqual(asyncio.run(clm_dispatch_execute(2102,'ok')), 'ok')
        self.assertEqual(clm_depend_dispose(),1)
        def failed(deps,payload): raise ValueError('boom')
        clm_dispatch_register_handler(2103,[2101],failed)
        with self.assertRaisesRegex(ValueError,'boom'):
            asyncio.run(clm_dispatch_execute(2103,None))
        self.assertEqual(clm_depend_dispose(),1)

if __name__ == '__main__': unittest.main()
