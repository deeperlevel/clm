import os
import tempfile
import unittest
from pathlib import Path
import clm_path as p
import clm_file as f
from clm_storage import *

class FileTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        p.clm_path_destroy();self.addCleanup(p.clm_path_destroy)
        self.file=Path(self.tmp.name)/'test.bin'
        self.file.write_bytes(b'abc\xffXYZ')
        p.clm_path_map(101,str(self.file))
        for sid,typ,sz in ((210,STORAGE_TYPE_BYTE,1),(211,STORAGE_TYPE_STRING,2048),(212,STORAGE_TYPE_STRING,2048),(213,STORAGE_TYPE_DYNAMIC,4)):
            self.assertEqual(clm_storage_create(sid,typ,size=sz,reserved=50),STORAGE_SUCCESS)
            self.addCleanup(lambda sid=sid: clm_storage_destroy(sid))

    def test_binary_roundtrip_and_insert(self):
        self.assertEqual(f.clm_file_load(101,210),7)
        self.assertEqual(clm_storage_get(210,3)[1],-1)
        f.clm_file_save(101,210)
        self.assertEqual(self.file.read_bytes(),b'abc\xffXYZ')
        clm_storage_clear(210);clm_storage_append(210,ord('Q'))
        f.clm_file_insert(101,210,3)
        self.assertEqual(self.file.read_bytes(),b'abcQ\xffXYZ')
        f.clm_file_prepend(101,210)
        f.clm_file_append(101,210)
        self.assertEqual(self.file.read_bytes(),b'QabcQ\xffXYZQ')
        f.clm_file_clear(101)
        self.assertEqual(f.clm_file_get_size(101),0)

    def test_stream_seek_and_auto_move(self):
        self.file.write_bytes(b'abcdefghij')
        f.clm_file_begin_streaming(300,101,4)
        self.assertEqual(f.clm_file_get_stream_info(300).block_count,3)
        self.assertEqual(f.clm_file_load_block(300,213),4)
        self.assertEqual(clm_storage_get(213,0)[1],b'abcd')
        self.assertEqual(f.clm_file_get_stream_info(300).seek_index,0)
        self.assertEqual(f.clm_file_load_block_append(300,213),4)
        self.assertEqual(clm_storage_count(213)[1],2)
        f.clm_file_seek_auto_move_enable(300)
        f.clm_file_seek_last(300)
        self.assertEqual(f.clm_file_load_block(300,213),2)
        self.assertEqual(clm_storage_get(213,0)[1],b'ij\x00\x00')
        self.assertEqual(f.clm_file_get_stream_info(300).last_read_size,2)
        self.assertEqual(f.clm_file_get_stream_info(300).seek_index,2)
        self.assertEqual(f.clm_file_load_block(300,213),2)
        with self.assertRaises(f.FileError): f.clm_file_seek_next(300)
        self.assertEqual(f.clm_file_get_stream_info(300).seek_index,2)
        f.clm_file_free(213)
        self.assertEqual(clm_storage_count(213)[1],0)
        f.clm_file_close(300)
        self.assertEqual(clm_storage_get_info(213)[0], STORAGE_SUCCESS)

    def test_stream_mismatch_and_empty(self):
        self.file.write_bytes(b'')
        f.clm_file_begin_streaming(301,101,4)
        try:
            self.assertEqual(f.clm_file_get_stream_info(301).seek_index,-1)
            with self.assertRaises(EOFError): f.clm_file_load_block(301,213)
        finally: f.clm_file_close(301)
        self.file.write_bytes(b'12345')
        f.clm_file_begin_streaming(302,101,8)
        try:
            clm_storage_append(213,b'abcd')
            with self.assertRaises(f.FileError): f.clm_file_load_block(302,213)
            self.assertEqual(clm_storage_get(213,0)[1],b'abcd')
        finally: f.clm_file_close(302)

    def test_metadata(self):
        self.assertTrue(f.clm_file_is_file_existing(101))
        self.assertEqual(f.clm_file_get_size(101),7)
        ms=f.clm_file_get_last_write_time(101)
        f.clm_file_set_last_write_time(101,ms-10000)
        self.assertAlmostEqual(f.clm_file_get_last_write_time(101),ms-10000,delta=1000)
        if os.name!='nt':
            with self.assertRaises(f.UnsupportedFilesystemOperation): f.clm_file_set_hidden(101,True)
            with self.assertRaises(f.UnsupportedFilesystemOperation): f.clm_file_set_creation_time(101,ms)

if __name__=='__main__': unittest.main()

class FileSafetyTests(unittest.TestCase):
    def test_read_only_and_invalid_stream_cursor(self):
        with tempfile.TemporaryDirectory() as tmp:
            p.clm_path_destroy()
            name = Path(tmp)/'readonly.bin'
            name.write_bytes(b'abcde')
            p.clm_path_map(5001, str(name))
            clm_storage_create(5002, STORAGE_TYPE_BYTE, reserved=8)
            clm_storage_append(5002, 65)
            try:
                name.chmod(0o444)
                with self.assertRaises(PermissionError):
                    f.clm_file_append(5001,5002)
                self.assertEqual(name.read_bytes(),b'abcde')
                f.clm_file_begin_streaming(5003,5001,4)
                try:
                    with self.assertRaises(f.FileError): f.clm_file_seek(5003,2)
                    self.assertEqual(f.clm_file_get_stream_info(5003).seek_index,0)
                finally: f.clm_file_close(5003)
            finally:
                name.chmod(0o644)
                clm_storage_destroy(5002)
                p.clm_path_destroy()
