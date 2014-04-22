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
    "include/slimsig/signal_base.h",
    "include/slimsig/connection.h"]
  }, {
    "target_name": "benchmark",
    "type": "executable",
    "include_dirs": ["benchmark", "include"],
    "includes": ["slimsig.gypi"],
    "sources": ["benchmark/benchmark.cpp"]
  }]
}
