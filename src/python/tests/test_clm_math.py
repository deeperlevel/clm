import unittest,math
from clm_storage import *
from _clm_math_core import *
from clm_math import *
from clm_math_vector import *
from clm_math_interpolation import *
from clm_math_matrix import *
from clm_math_quaternion import *
from clm_math_geometry import *
from clm_math_collision import *
from clm_math_number import *
from clm_math_random import *

class CLMMathTests(unittest.TestCase):
    def setUp(self):
        self.ids=list(range(90000,90028))
        for i in self.ids:
            self.assertEqual(clm_storage_create(i,STORAGE_TYPE_DOUBLE),STORAGE_SUCCESS)
    def tearDown(self):
        for i in self.ids:clm_storage_destroy(i)
    def put(self,i,*v):self.assertEqual(write(self.ids[i],v),0);return self.ids[i]
    def get(self,i):return read(self.ids[i])[1]
    def test_scalar(self):
        self.assertEqual(clm_math_clamp(9,0,5),5)
        self.assertAlmostEqual(clm_math_lerp(2,10,.25),4)
        self.assertEqual(clm_math_inverse_lerp(0,10,5),.5)
        self.assertEqual(clm_math_smoothstep(0,1,.5),.5)
        self.assertAlmostEqual(clm_math_sin(clm_math_radians(90)),1)
        self.assertEqual(clm_math_wrap(13,0,10),3)
        with self.assertRaises(ValueError):clm_math_inverse_lerp(3,3,3)
    def test_vector(self):
        a=self.put(0,3.,4.);b=self.put(1,1.,2.);o=self.ids[2]
        self.assertEqual(clm_math_vector_add(a,b,o),0);self.assertEqual(self.get(2),[4,6])
        self.assertEqual(clm_math_vector_length(a),(0,5.))
        self.assertEqual(clm_math_vector_dot(a,b),(0,11.))
        self.assertEqual(clm_math_vector_normalize(a,a),0)
        self.assertAlmostEqual(self.get(0)[0],.6)
        self.assertEqual(clm_math_vector_normalize(self.put(3,0.,0.),o),MATH_ERROR_DOMAIN)
        self.assertEqual(self.get(2),[4,6])
        self.assertEqual(clm_math_vector_lerp(b,self.put(4,3.,6.),.5,o),0)
        self.assertEqual(self.get(2),[2,4])
        self.assertEqual(clm_math_vector_cross(self.put(5,1.,0.,0.),self.put(6,0.,1.,0.),o),0)
        self.assertEqual(self.get(2),[0,0,1])
    def test_interpolation(self):
        self.assertEqual(clm_math_interpolation_ease_in_out(.5),.5)
        self.assertEqual(clm_math_interpolation_cubic(0,10,20,30,.5),15)
        self.assertEqual(clm_math_interpolation_storage_lerp(self.put(0,0.,2.),self.put(1,4.,6.),.25,self.ids[2]),0)
        self.assertEqual(self.get(2),[1,3])
    def test_matrix(self):
        a=self.put(0,4.,7.,2.,6.);o=self.ids[2]
        self.assertEqual(clm_math_matrix_determinant(a,2),(0,10.))
        self.assertEqual(clm_math_matrix_inverse(a,2,o),0)
        self.assertEqual(clm_math_matrix_multiply(a,2,2,o,2,2,self.ids[3]),0)
        for v,expected in zip(self.get(3),[1.,0.,0.,1.]):self.assertAlmostEqual(v,expected)
        self.assertEqual(clm_math_matrix_multiply(a,2,2,a,2,2,a),0) # alias
        self.assertEqual(self.get(0),[30,70,20,50])
        self.assertEqual(clm_math_matrix_inverse(self.put(4,1.,2.,2.,4.),2,o),MATH_ERROR_SINGULAR)
        self.assertEqual(clm_math_matrix_translation2d(3,4,o),0)
        self.assertEqual(clm_math_matrix_transform_vector(o,3,3,self.put(5,2.,2.,1.),self.ids[6]),0)
        self.assertEqual(self.get(6),[5,6,1])
    def test_quaternion(self):
        q=self.ids[0];self.assertEqual(clm_math_quaternion_identity(q),0)
        v=self.put(1,1.,2.,3.);out=self.ids[2]
        self.assertEqual(clm_math_quaternion_rotate_vector(q,v,out),0)
        self.assertEqual(self.get(2),[1,2,3])
        axis=self.put(3,0.,0.,1.)
        self.assertEqual(clm_math_quaternion_from_axis_angle(axis,math.pi,q),0)
        self.assertEqual(clm_math_quaternion_rotate_vector(q,self.put(4,1.,0.,0.),out),0)
        self.assertAlmostEqual(self.get(2)[0],-1)
        self.assertEqual(clm_math_quaternion_slerp(self.ids[5],q,.5,out),MATH_ERROR_DIMENSION)
        self.assertEqual(clm_math_quaternion_identity(self.ids[5]),0)
        self.assertEqual(clm_math_quaternion_slerp(self.ids[5],q,.5,out),0)
        self.assertAlmostEqual(sum(x*x for x in self.get(2)),1.)
    def test_geometry(self):
        p=self.put(0,1.,1.);a=self.put(1,0.,0.);b=self.put(2,2.,0.)
        self.assertEqual(clm_math_geometry_distance_point_segment(p,a,b),(0,1.))
        self.assertEqual(clm_math_geometry_closest_point_segment(p,a,b,self.ids[3]),0)
        self.assertEqual(self.get(3),[1,0])
        poly=self.put(4,0.,0.,2.,0.,2.,2.,0.,2.)
        self.assertEqual(clm_math_geometry_polygon_area(poly),(0,4.))
        self.assertEqual(clm_math_geometry_point_in_polygon(p,poly),(0,True))
        self.assertEqual(clm_math_geometry_point_in_polygon(self.put(5,0.,1.),poly),(0,True))
        self.assertEqual(clm_math_geometry_point_in_polygon(self.put(6,3.,3.),poly),(0,False))
    def test_collision(self):
        a=self.put(0,0.,0.,2.,2.);b=self.put(1,2.,2.,5.,5.);c=self.put(2,2.,2.,1.)
        self.assertEqual(clm_math_collision_aabb_aabb(a,b),(0,True))
        self.assertEqual(clm_math_collision_circle_aabb(c,a),(0,True))
        self.assertEqual(clm_math_collision_point_circle(self.put(3,2.,3.),c),(0,True))
        self.assertEqual(clm_math_collision_ray_aabb(self.put(4,-2.,1.),self.put(5,1.,0.),a),(0,2.))
        self.assertEqual(clm_math_collision_ray_aabb(self.put(4,-2.,1.),self.put(6,0.,1.),a),(0,None))
    def test_number(self):
        self.assertEqual(clm_math_number_gcd(48,18),6)
        self.assertEqual(clm_math_number_lcm(6,8),24)
        g,x,y=clm_math_number_extended_gcd(240,46)
        self.assertEqual((g,240*x+46*y),(2,2))
        self.assertEqual(clm_math_number_mod_pow(4,13,497),445)
        self.assertEqual(clm_math_number_mod_inverse(3,11),4)
        self.assertEqual(clm_math_number_mod(-1,7),6)
        self.assertTrue(clm_math_number_is_prime(97))
        self.assertFalse(clm_math_number_is_prime(1))
        self.assertEqual(clm_math_number_next_prime(97),101)
        sid=99001
        try:
            self.assertEqual(clm_storage_create(sid,STORAGE_TYPE_LONG_INTEGER),0)
            self.assertEqual(clm_math_number_prime_factors(84,sid),4)
            self.assertEqual([clm_storage_get(sid,i)[1] for i in range(4)],[2,2,3,7])
        finally:clm_storage_destroy(sid)
    def test_random(self):
        a,b=99002,99003
        try:
            for sid in (a,b):
                self.assertEqual(clm_storage_create(sid,STORAGE_TYPE_LONG_INTEGER),0)
                self.assertEqual(clm_storage_resize(sid,2),0)
                clm_math_random_seed(sid,42,54)
            seq=[clm_math_random_uint32(a) for _ in range(5)]
            self.assertEqual(seq,[clm_math_random_uint32(b) for _ in range(5)])
            self.assertEqual(seq[0],0xa15c02b7)
            for _ in range(20):self.assertTrue(3<=clm_math_random_integer(a,3,8)<=8)
        finally:
            clm_storage_destroy(a);clm_storage_destroy(b)
    def test_invalid_resources(self):
        self.assertEqual(clm_math_vector_add(999999,self.ids[0],self.ids[1]),MATH_ERROR_RESOURCE)
        self.assertEqual(clm_math_vector_add(self.put(0,1.,2.),self.put(1,1.,2.,3.),self.ids[2]),MATH_ERROR_DIMENSION)
        self.assertEqual(clm_math_matrix_identity(self.ids[0],0),MATH_ERROR_DIMENSION)

if __name__=='__main__': unittest.main()
