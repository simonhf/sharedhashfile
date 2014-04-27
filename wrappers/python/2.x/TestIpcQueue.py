import SharedHashFile
import unittest
from os import system


class TestSharedHashFile(unittest.TestCase):
    def test_canAttachToSharedMemory(self):
        self.assertTrue(SharedHashFile.attach('/dev/shm', 'test-luke1'))
        
    def test_canAttachToExistingSharedMemory(self):
        SharedHashFile.attach('/dev/shm', 'test-luke1')
        self.assertTrue(SharedHashFile.attachExisting('/dev/shm', 'test-luke1'))
        
    def test_setdebuggingLogging(self):
        SharedHashFile.debugVerbosityLess()
        pass

    def tearDown(self):
        unittest.TestCase.tearDown(self)
        system("rm -rf /dev/shm/test-luke1")
        
        
if __name__ == "__main__":
    #import sys;sys.argv = ['', 'Test.testName']
    unittest.main()
    
    