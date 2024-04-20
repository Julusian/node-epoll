{
  "targets": [{
    "target_name": "epoll",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "xcode_settings": { "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7",
      },
      "msvs_settings": {
        "VCCLCompilerTool": { "ExceptionHandling": 1 },
      },
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "xcode_settings": { "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7",
      },
      "msvs_settings": {
        "VCCLCompilerTool": { "ExceptionHandling": 1 },
      },
    "conditions": [[
      'OS == "linux"', {
        'dependencies': [
          "<!(node -p \"require('node-addon-api').targets\"):node_addon_api_except",
        ],
        "sources": [
          "./src/epoll.cc",
          "./src/watcher.cc"
        ],
        "conditions": [[
          '"<!(echo $V)" != "1"', {
            "cflags": [
              "-Wno-deprecated-declarations",
              "-Wno-cast-function-type"
            ]
          }]
        ]
      }]
    ]
  }]
}

