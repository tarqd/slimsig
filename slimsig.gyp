{
  "includes": ["common.gypi"],
  "targets": [{
    "target_name": "test-runner",
    "type": "executable",
    "include_dirs": ["deps/bandit", "test"],
    "includes": ["slimsig.gypi"],
    "sources": ["test/test.cpp",
    # for ease of development
    "include/slimsig/slimsig.h",
    "include/slimsig/tracked_connect.h",
    "include/slimsig/detail/signal_base.h",
    "include/slimsig/connection.h",
    "include/slimsig/detail/slot.h",
    "slimsig.gyp", "slimsig.gypi"]
  }, {
    "target_name": "benchmark",
    "type": "executable",
    "include_dirs": ["benchmark", "include"],
    "includes": ["slimsig.gypi"],
    "sources": ["benchmark/benchmark.cpp"]
  }, {
    "target_name": "benchmark-boost",
    "type": "executable",
    "include_dirs": ["benchmark", "include", "/usr/local/include/"],
    "includes": ["slimsig.gypi"],
    "sources": ["benchmark/benchmark-boost.cpp"]
  }]
}
