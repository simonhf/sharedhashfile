{
  'conditions': [
      ['OS=="linux"', {
        'targets': [
          {
            "target_name": "SharedHashFile",
              'include_dirs': [
                '../..',
              ],
              "configurations": {
                # This augments the `Debug` configuration used when calling `node-gyp`
                # with `--debug` to create code coverage files used by lcov
                "Debug": {
                  "conditions": [
                    ['OS=="linux"', {
                      "cflags": ["-std=gnu++11 -DSHF_DEBUG_VERSION"],
                    }],
                  ]
                },
                "Release": {
                  "conditions": [
                    ['OS=="linux"', {
                      "cflags": ["-std=gnu++11"],
                    }],
                  ]
                },
              },
              "sources": [
                "SharedHashFile.cc",
              ],
              "libraries": [ "../../../$(SHF_BUILD_TYPE)/SharedHashFile.a", ],
          },
        ],
      }],
   ],
}
