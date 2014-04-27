import SharedHashFile
import unittest
from os import system, getpid

SharedFileName = 'test-python-' + str(getpid()) 

class TestSharedHashFile(unittest.TestCase):
    def test_canAttachToSharedMemory(self):
        self.assertTrue(SharedHashFile.attach('/dev/shm', SharedFileName, 1))
        
    def test_canAttachToExistingSharedMemory(self):
        SharedHashFile.attach('/dev/shm', SharedFileName, 1)
        self.assertTrue(SharedHashFile.attachExisting('/dev/shm', SharedFileName))
        
    def test_canBuildNewQueue(self):
        SharedHashFile.attach('/dev/shm', SharedFileName, 1)
        testQs         = 3
        testQItems     = 10
        testQItemSize  = 4096
        testQidsNolockMax = 1
        SharedHashFile.qNew(testQs, testQItems, testQItemSize, testQidsNolockMax)
        self.assertGreater(SharedHashFile.qNewName("Testing"), -1)
        
    def test_PushItemsFromAtoBautomatically(self):
        SharedHashFile.attach('/dev/shm', SharedFileName, 1)
        testQs         = 3
        testQItems     = 2
        testQItemSize  = 4096
        testQidsNolockMax = 1
        invalid = 4294967295
        SharedHashFile.qNew(testQs, testQItems, testQItemSize, testQidsNolockMax)
        source = SharedHashFile.qNewName("TestingAtoB")
        dest = SharedHashFile.qNewName("TestingBtoA")
        
        nextItem = SharedHashFile.qPushHeadPullTail(dest, invalid, source)
        
        nextItem = SharedHashFile.qPushHeadPullTail(dest, nextItem, source)
        self.assertNotEqual(nextItem, invalid)
        
        nextItem = SharedHashFile.qPushHeadPullTail(dest, nextItem, source)
        self.assertEqual(nextItem, invalid)
        
    def test_setdebuggingLogging(self):
        SharedHashFile.debugVerbosityLess()
        pass

    def tearDown(self):
        unittest.TestCase.tearDown(self)
        if SharedHashFile.isAttached():
            SharedHashFile.delete()
        
if __name__ == "__main__":
    #import sys;sys.argv = ['', 'Test.testName']
    unittest.main()
    
    