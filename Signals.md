# Signal / Slots

Signals and slots are a powerful way to implement an Actor/Observer model in C++. Some of the more popular implementations such as boost::signals2 and sigc++ are feature rich and easy to use but have a reputation for being a bit expensive and slow. Let's dive into why that is.

# Linked Lists and Heap Allocations
Both boost::signals2 and sigc++ used linked lists as the data structure under
the hood to store the list of slots or callbacks. This forces an allocation for
each time you connect a slot which isn't too bad in the scheme of things
because generally you're only connecting a couple of signals to each slot.
However, this has dramatic effects on the speed of iteration when emitting your
signal. Every time you fetch the next item in the list your poor CPU has to
search through memory, most likely triggering cache misses because of the
fragmentation caused by linked lists. Using a vector dramatically reduces the
cost of iterating by keeping everything contiguous in memory. 

# Disconnecting slots
Obviously the nice part about linked lists is your iterators stay valid no
matter how you modify the list. As it turns out though, most of the time once
you've setup your slots you won't need to modify them nearly as much as you'll
be emitting your signal so speed isn't really a concern here. Since most of the
time you only have a few slots anyway just keeping a unique id assigned to each
slot and using a binary search will allow you to find and remove slots easily
without much cost in performance on disconnecting while gaining huge while
emitting. The only gotcha with using vectors is that you have to mark slots for
deletion instead of actually removing them when the signal is current emitting.


