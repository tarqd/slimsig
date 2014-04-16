slimsig
=======

A light-weight alternative to Boost::Signals2 and SigSlot++

## What makes this different from all the other Signal/Slot libraries out there?
 - Light-weight: No unnecessary virtual calls besides the std::function 
 - Uses vectors as the underlying storage
    The theory is that you'll be emitting signals far more than you'll be adding slots 
    so using vectors will improve performance greatly by playing nice with the CPUs cache and taking
    advantage of SIMD
 - Supports adding/removing slots while the signal is running (despite it being a vector)
 - Removing slots should still be fast as it uses std::remove_if to iterate/execute slots
    Meaning that if there are disconnected slots they are removed quickly after iteration
 - Custom allocators for more performance tweaking

## How to use it
`slimgsig` is a header only library. Just add `include/slimsig/slimgsig.h` to your project and you're good to go. I'd reccomend adding the path to `include` to your header search directories so you can include it as `<slimgsig/slimgsig.h>` similiar to how you would use boost and other libraries.

## How to use with gyp
If you're using gyp you can simply add `"includes": ["path/to/slimsig.gypi"]` to your target and the appropriate header search paths will be added for you

## How to run the unit tests
If you using gyp just execute `$ gyp --depth=./ slimsig.gyp` from the command line to generate an project file for your platform. It defaults to xcode on OSX, Visual Studio on Windows and a makefile on linux/posix systems but you can choose which one you want with `-f [projecttype]`. See the gyp documentation for more details.

Once you've got your project just open it up and build the `test-runner` target and run it. We use bandit as a unit-test framework so you can execute a bunch of neat commands like `./test-runner --reporter=spec --only connection` to only run connection related tests and show the output as in a neat doc-like format. For more information see the [bandit documentation](http://banditcpp.org/).

## How do I get started with slimsignal? 
The usage is pretty much the same as Boost::Signals2. There's a `signal` class, `connection`, and `scoped_connection` that work in very much the same way minus a lot of the extra (but heavy) features boost includes such as object tracking or signals that return values.

I'll write up some proper documentation soon but for now check out `test/test.cpp` for some examples.
 
## While still being light-weight it still supports:
 - connections/scoped_connections
 - connection.disconnect()/connection.connected() can be called after the signal stops existing 
    There's a small overhead because of the use of a shared_ptr for each slot, however, my average use-case
    Involves adding 1-2 slots per signal so the overhead is neglible, especially if you use a custom allocator such as boost:pool

  To keep it simple I left out support for slots that return values, though it shouldn't be too hard to implement.
  I've also left thread safety as something to be handled by higher level libraries
  Much in the spirit of other STL containers. My reasoning is that even with thread safety sort of baked in
  The user would still be responsible making sure slots don't do anything funny if they are executed on different threads
  All the mechanics for this would complicate the library and really not solve much of anything while also slowing down 
  the library for applications where signals/slots are only used from a single thread
 
  Plus it makes things like adding slots to multiple signals really slow because you have to lock each and every time
  You'd be better off using your own synchronization methods to setup all your signals at once and releasing your lock after
  you're done
