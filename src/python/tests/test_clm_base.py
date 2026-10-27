import pathlib
import sys
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'python'))
import clm.clm_base

class TestClmBase(unittest.TestCase):
    pass

if __name__ == '__main__': unittest.main()
