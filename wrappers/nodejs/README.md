# SharedHashFile for Node.js

These files contain the wrapper code to use SharedHashFile from javascript in Node.js.

## Experiments regarding the speed of moving data between C++ and V8

[SharedHashFile.cc](SharedHashFile.cc) contains dummy functions, each with slightly different behaviour varying from doing nothing to lots of input & output, but without any business logic code. [SharedHashFileDummy.js](SharedHashFileDummy.js) then calls each function many times and estimates how many times per second the function could be called. In this way it's possible to determine the relative overhead for adding more and more 'scafolding' to a particular C++ function.

The results show that V8 is not particularly fast at calling C++ functions. The fastest possible function -- which does nothing -- only manages 25 million calls per second from V8. Whereas, the same function would manage 850 million calls per second from C++. So before we add any kind of input or output to our C++ function then we're already 34 times slower.

When adding input and output to our C++ function then things get slower still. V8 isolation by design stops C++ from modifying even the simpliest integer passed to C++ as an argument. This is because integers are never passed to C++ as an argument. Instead C++ gets an object with an interface with which the integer can be retrieved. Unfortunately those objects always seem to be immutable, so there is pretty much never an object function to e.g. set the integer value. This can result in a bottleneck whereby your C++ function wants to return more than one output. As a developer you then have a choice: Create more C++ functions to return the other outputs but at the cost of having to call those functions through the slow V8 to C++ interface. Or, output V8 objects, e.g. an array of output objects; in the experiment below then there's little performance advantage to making 1x C++ call and returning an array of 10 external strings, or making 10x C++ calls each returning 1 external string.

Moving strings in and out of V8 is particularly expensive presumably because of all the object construction and copying of the string data itself? However, there does seem to be two possible ways to optimize string passing: (1) Use ```String::NewExternal()``` which appears to be a special case in the V8 world whereby an object is still created but references memory for the string data which is outside the V8 memory space, thus providing a kind of zero copy (except for the container object creation) string mechanism from C++ to V8. However, the external string is then read-only via javascript inside V8. (2) ```Buffer::New()``` promises to do something similar to ```String::NewExternal()``` in that it will take a C++ string without copying it, but then javascript can modify the string with subsequent ```Buffer``` member function calls. However, the perf tests below show that ```Buffer::New()``` is about 10x slower than ```String::NewExternal()```, so is it really useful for high performance?

```
$ make clean ; rm -rf /dev/shm/test-*/ ; SHF_ENABLE_PERFORMANCE_TEST=0 make
...
make: running test: perf test calling dummy C++ functions
1..12
nodejs: debug: about to require  SharedHashFile
nodejs: debug:          required SharedHashFile
ok 1 - nodejs: did expected number of .dummy1()  calls  // estimate 25,000,322 calls per second; object: n/a, input: n/a, output: n/a
ok 2 - nodejs: did expected number of .dummy2()  calls  // estimate 20,000,496 calls per second; object: unwrapped, input: n/a, output: n/a
ok 3 - nodejs: did expected number of .dummy3a() calls  // estimate 16,666,550 calls per second; object: unwrapped, input: 1 int, output: n/a
ok 4 - nodejs: did expected number of .dummy3b() calls  // estimate 14,285,777 calls per second; object: unwrapped, input: 3 ints, output: n/a
ok 5 - nodejs: did expected number of .dummy4()  calls  // estimate 10,000,010 calls per second; object: unwrapped, input: 3 ints, output: int
ok 6 - nodejs: did expected number of .dummy5()  calls  // estimate 5,882,284 calls per second; object: unwrapped, input: 3 ints, output: 8 byte str
ok 7 - nodejs: did expected number of .dummy6()  calls  // estimate 1,315,790 calls per second; object: unwrapped, input: 3 ints, output: 4KB byte str
ok 8 - nodejs: did expected number of .dummy7a() calls  // estimate 5,000,005 calls per second: object: unwrapped, input: 3 ints, output: 4KB byte str external
ok 9 - nodejs: did expected number of .dummy7b() calls  // estimate 2,857,136 calls per second: object: unwrapped, input: 3 ints, output: 4KB byte str external x1 in array
ok 10 - nodejs: did expected number of .dummy7c() calls  // estimate 609,756 calls per second: object: unwrapped, input: 3 ints, output: 4KB byte str external x10 in array
ok 11 - nodejs: did expected number of .dummy8()  calls  // estimate 321,544 calls per second: object: unwrapped, input: 3 ints, output: 4KB byte buffer
ok 12 - nodejs: did expected number of .dummy9()  calls  // estimate 549,450 calls per second: object: unwrapped, input: 3 ints, output: 4KB byte buffer external
...
```
