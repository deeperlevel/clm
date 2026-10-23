"""Contract tests for CLM Depend's tracked ID-based instance model."""
import asyncio
import unittest
from clm_depend import (Depend, DependError, DependLifetime as L,
    DependStatus as S, clm_depend_get_info, DEPEND_INSTANCE_CONTAINER_ID)
from clm_container import clm_container_count, CONTAINER_SUCCESS

class DependTests(unittest.TestCase):
    def setUp(self): Depend.destroy(); Depend.initialize()
    def tearDown(self): Depend.destroy()

    def test_eager_and_lazy_caching(self):
        calls=[]
        self.assertEqual(Depend.register(1, [], lambda _: calls.append(1) or 17), S.SUCCESS)
        self.assertEqual(calls,[1]); a=Depend.resolve(1)
        self.assertEqual(a,Depend.resolve(1)); self.assertEqual(Depend.get(a),17)
        self.assertEqual(Depend.register(2,[],lambda _: calls.append(2) or None,L.LAZY),S.SUCCESS)
        self.assertEqual(calls,[1]); b=Depend.resolve(2)
        self.assertIsNone(Depend.get(b)); self.assertEqual(Depend.resolve(2),b)
        self.assertEqual(calls,[1,2])
        self.assertEqual(Depend.dispose(),0)

    def test_transient_unique_ids_and_deferred_disposal(self):
        self.assertEqual(Depend.register(10,[],lambda _: {'n':1},L.TRANSIENT),S.SUCCESS)
        a,b=Depend.resolve(10),Depend.resolve(10)
        self.assertNotEqual(a,b); self.assertIsNot(Depend.get(a),Depend.get(b))
        self.assertEqual(Depend.get_info(a).references,1)
        Depend.reference(a); self.assertEqual(Depend.get_info(a).references,2)
        Depend.unreference(a); self.assertEqual(Depend.dispose(),0)
        Depend.unreference(a); self.assertEqual(Depend.get_info(a).references,0)
        self.assertEqual(Depend.get(a),{'n':1})
        self.assertEqual(Depend.dispose(),1)
        with self.assertRaises(DependError) as ctx: Depend.get(a)
        self.assertEqual(ctx.exception.status,S.INSTANCE_NOT_FOUND)
        self.assertEqual(Depend.get_info(b).references,1)

    def test_underflow_and_stale_id(self):
        Depend.register(10,[],lambda _: object(),L.TRANSIENT)
        a=Depend.resolve(10); Depend.unreference(a)
        with self.assertRaises(DependError) as ctx: Depend.unreference(a)
        self.assertEqual(ctx.exception.status,S.REFERENCE_UNDERFLOW)
        Depend.dispose()
        with self.assertRaises(DependError): Depend.reference(a)
        b=Depend.resolve(10); self.assertNotEqual(a,b)

    def test_dependency_ids_and_recursive_disposal(self):
        disposed=[]
        Depend.register(1,[],lambda _: {'x':3},L.TRANSIENT,destructor=lambda v: disposed.append('component'))
        Depend.register(2,[1],lambda deps: {'component_id':deps[0]},L.TRANSIENT,
                        destructor=lambda v: disposed.append('entity'))
        parent=Depend.resolve(2); child=Depend.get(parent)['component_id']
        self.assertEqual(Depend.get(child),{'x':3})
        self.assertEqual(Depend.get_info(parent).dependencies,(child,))
        Depend.unreference(parent)
        self.assertEqual(Depend.dispose(),2)
        self.assertEqual(disposed,['entity','component'])

    def test_shared_dependency_references(self):
        Depend.register(1,[],lambda _: 7,L.LAZY)
        Depend.register(2,[1],lambda deps: deps[0],L.TRANSIENT)
        singleton=Depend.resolve(1)
        parent=Depend.resolve(2)
        self.assertEqual(Depend.get(parent),singleton)
        self.assertEqual(Depend.get_info(singleton).references,2)
        Depend.unreference(parent); self.assertEqual(Depend.dispose(),1)
        self.assertEqual(Depend.get_info(singleton).references,1)
        self.assertEqual(Depend.dispose(),0)

    def test_scope_lifetime(self):
        deleted=[]
        Depend.register(1,[],lambda _: object(),L.SCOPED,destructor=lambda _: deleted.append(1))
        with self.assertRaises(DependError) as ctx: Depend.resolve(1)
        self.assertEqual(ctx.exception.status,S.SCOPE_REQUIRED)
        Depend.begin_scope(); a=Depend.resolve(1)
        self.assertEqual(a,Depend.resolve(1))
        self.assertEqual(Depend.dispose(),0)
        Depend.end_scope(); self.assertEqual(deleted,[1])
        with self.assertRaises(DependError): Depend.get(a)
        Depend.begin_scope(); b=Depend.resolve(1); self.assertNotEqual(a,b)
        Depend.end_scope()

    def test_nested_scope_parent_and_cleanup(self):
        Depend.register(1,[],lambda _: 3,L.TRANSIENT)
        Depend.register(2,[1],lambda ids: ids[0],L.SCOPED)
        Depend.begin_scope(); parent=Depend.resolve(2); child=Depend.get(parent)
        self.assertEqual(Depend.get_info(child).references,1)
        Depend.end_scope()
        self.assertEqual(Depend.get_info(child).references,0)
        self.assertEqual(Depend.dispose(),1)

    def test_scope_in_use(self):
        Depend.register(1,[],lambda _: 4,L.SCOPED)
        Depend.begin_scope(); iid=Depend.resolve(1)
        Depend.reference(iid)
        with self.assertRaises(DependError) as ctx: Depend.end_scope()
        self.assertEqual(ctx.exception.status,S.INSTANCE_IN_USE)
        Depend.unreference(iid); Depend.end_scope()

    def test_failed_eager_registration_rolls_back(self):
        self.assertEqual(Depend.register(10,[99],lambda _: 1),S.MISSING_DEPENDENCY)
        self.assertEqual(Depend.register(10,[],lambda _: 1),S.SUCCESS)
        with self.assertRaises(ValueError): Depend.register(11,[],lambda _: (_ for _ in ()).throw(ValueError()))
        self.assertEqual(Depend.register(11,[],lambda _: 3),S.SUCCESS)

    def test_circular_and_incompatible(self):
        Depend.register(1,[2],lambda _: 1,L.LAZY)
        Depend.register(2,[1],lambda _: 2,L.LAZY)
        with self.assertRaises(DependError) as ctx: Depend.resolve(1)
        self.assertEqual(ctx.exception.status,S.CIRCULAR_DEPENDENCY)
        Depend.register(3,[],lambda _: 3,L.SCOPED)
        self.assertEqual(Depend.register(4,[3],lambda _: 4),S.INCOMPATIBLE_LIFETIME)
        Depend.register(5,[3],lambda _: 5,L.LAZY)
        Depend.begin_scope()
        with self.assertRaises(DependError) as ctx: Depend.resolve(5)
        self.assertEqual(ctx.exception.status,S.INCOMPATIBLE_LIFETIME)
        Depend.end_scope()

    def test_construct_seal_unregister(self):
        Depend.register(1,[],lambda _: 1,L.LAZY)
        Depend.construct(1,lambda _: 2)
        self.assertEqual(Depend.get(Depend.resolve(1)),2)
        with self.assertRaises(DependError): Depend.construct(1,lambda _: 3)
        Depend.unregister(1)
        with self.assertRaises(DependError): Depend.resolve(1)
        Depend.register(1,[],lambda _: 5)
        Depend.seal()
        self.assertEqual(Depend.register(2,[],lambda _: 2),S.SEALED)
        with self.assertRaises(DependError): Depend.unregister(1)

    def test_invoke_temporary_transient_ownership(self):
        Depend.register(1,[],lambda _: {'a':8},L.TRANSIENT)
        iid=Depend.invoke([1],lambda handle: handle)
        self.assertEqual(Depend.get_info(iid).references,0)
        self.assertEqual(Depend.dispose(),1)
        async def func(handle, plus):
            await asyncio.sleep(0)
            self.assertEqual(Depend.get_info(handle).references,1)
            return Depend.get(handle)['a']+plus
        self.assertEqual(asyncio.run(Depend.invoke_async([1],func,4)),12)
        self.assertEqual(Depend.dispose(),1)

    def test_destructor_error_can_retry(self):
        fails=[]
        def destructor(_):
            fails.append(1)
            if len(fails)==1: raise RuntimeError('temporary failure')
        Depend.register(1,[],lambda _: 1,L.TRANSIENT,destructor=destructor)
        iid=Depend.resolve(1); Depend.unreference(iid)
        with self.assertRaises(RuntimeError): Depend.dispose()
        self.assertEqual(Depend.get_info(iid).references,0)
        self.assertEqual(Depend.dispose(),1)

    def test_invalid_registration_and_id_uniqueness(self):
        self.assertEqual(Depend.register(-1,[],lambda _: None),S.INVALID_ARGUMENT)
        self.assertEqual(Depend.register(1,[],lambda _: 1),S.SUCCESS)
        self.assertEqual(Depend.register(1,[],lambda _: 2),S.ALREADY_REGISTERED)
        with self.assertRaises(DependError): Depend.get(123)

if __name__=='__main__': unittest.main()
