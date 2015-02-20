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

console.log('disconnect all');
em.removeAllListeners('foo');
em.depth = 0;
var emit_original = em.emit;
em.emit = function () {
  em.depth++;
  var args = new Array(arguments.length);
  emit_original.apply(em, arguments);
  em.depth--;
};
function log() {
  console.log.apply(console, [em.depth + ": "].concat([].slice.apply(arguments)));
}
em.on('foo', function() {
  log('call 1');
  em.removeAllListeners('foo');
  em.on('foo', function () {
    log('call 3');
  })
  em.emit('foo');
});
em.on('foo', function () {
  log('call 2');
})
em.emit('foo');
