var em = new (require('events').EventEmitter)();

var count = 0;
em.on('foo', function() {
  ++count;
  if (count == 1) {
    em.once('foo', function(){
      ++count;
    });
    em.emit('foo');
  }
});

em.emit('foo');
console.log('count: ', count);
