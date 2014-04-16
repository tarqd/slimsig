{
  "includes": ["common.gypi"],
  "targets": [{
    "target_name": "test-runner",
    "type": "executable",
    "include_dirs": ["deps/bandit", "test"],
    "includes": ["slimsig.gypi"],
    "sources": ["test/test.cpp",
    # for ease of development
    "include/slimsig/slimsig.h"]
  }]
}
