# Pledge - C++ promise / future library

Pledge is a lightweight header-only promise / future C++17 library for writing
clean asynchronous code.

## Basic usage

Promises are used to set the value and create a linked future. Futures are used
to read the value and add continuations with `then`:

```c++
Pledge::Promise<int> promise;
promise.future().then([] (int value) {
  assert(value == 42);
});
promise.setValue(42);
```

The future didn't have an executor, so calling `setValue` immediately triggered
the continuation callback. To make it asynchronous, let's use a thread pool.
This time we don't need to create a Promise explicitly, but we create a Future
with `Pledge::via`.

```c++
Pledge::ThreadPoolExecutor threadPool;

Pledge::via(&threadPool, [] {
  // This is called in a worker thread in the thread pool
  return calculateSomethingExpensive();
}).via(&mainThread).then([] (int value) {
  // via call transferred the future to another thread, so now this is
  // executed in mainThread.
  useTheValueInMainThread(value);
});
```

## Blocking wait

Use `get()` to wait and move the result out of the future. Calculate 1 + 1
in a worker thread, wait for the result to complete and assign it to `value`:

```c++
int value = Pledge::via(&threadPool, [] {
  return 1 + 1;
}).get();
```

Notice that if the future has an error, calling `get()` will throw that error.

## Futures and promises without a value

`Promise<void>` and `Future<void>` (or just `Promise<>` and `Future<>`) are
useful when the future doesn't need to return anything, but you can use it to
track when an asynchronous operation finishes.

You can mix void futures with typed futures. Calling `.get()` to a void future
doesn't return anything, it just waits for the future to finish:

```c++
Pledge::via(&threadPool, [] {
  // Do nothing
}).then([] {
  return 1;
}).then([] (int value) {
  return std::to_string(value);
}).then([] (std::string str) {
  printf("str: %s\n", str.c_str());
}).via(&mainThread).then([] {
  printf("all done\n");
}).get();
```

## Error handling

Error handling is done with exceptions, but without explicitly needing to write
`try` / `catch` anywhere. You can throw anything from a continuation callback
to set the future to an error state. You can handle the error using `error()`
continuation:

```c++
Pledge::via(&threadPool, [] {
  throw "Take this";
}).then([] {
  // This is not called.
}).error([] (const char* err) {
  printf("Someone threw a string: %s\n", err);
});
```

Perhaps typically you would throw standard exceptions and have multiple error
handlers:

```c++
Pledge::via(&threadPool, [] {
  // This throws std::out_of_range
  return std::vector<int>().at(1);
}).then([] (int value) {
  // This is not called.
}).error([] (const std::runtime_error& err) {
  // This is also not called, since std::out_of_range is not a runtime_error.
}).error([] (const std::logic_error& err) {
  // This is called, since std::out_of_range inherits from logic_error.
  fprintf(stderr, "Error: %s\n", err.what());
});
```

Error handlers can also return a value, which will make the future valid again:

```c++
Pledge::via(&threadPool, [] {
  return std::vector<int>().at(1);
}).error([] (const std::exception& err) {
  fprintf(stderr, "Warning: %s - using default value 42 instead\n", err.what());
  return 42;
}).then([] (int v) {
  // Previous error handler returned a value so this is called. If the first
  // lambda wouldn't throw anything but just return a value, this would be
  // called directly without the error handler.
  assert(v == 42);
});
```

You can also set the error using the promise:

```c++
Pledge::Promise<int> promise;

promise.setError(std::runtime_error("No."));

// or:

try {
  promise.setValue(doStuffThatMightThrow());
} catch (...) {
  promise.setError(std::current_exception());
}

// or just:
promise.set([] { return doStuffThatMightThrow(); });
```

## Returning futures from then()/error()

Continuations can also return futures, and those are just flattened to
the future chain.

For instance, consider this HTTP request api that has an async function for
fetching content and another async function for parsing the content as JSON:

```c++
struct Response
{
  Pledge::Future<JSON> json();
};
Pledge::Future<Response> fetch(const std::string& url);

fetch(url).then([] (Response r) {
  // r.json() returns Future<JSON> but that is flattened automatically and
  // the return value of this lambda is Future<JSON>, not Future<Future<JSON>>,
  // so the next `then` continuation is called when the json is ready.
  return r.json();
}).then([] (JSON json) {
  processJson(json);
}).error([] (const std::exception& e) {
  fprintf(stderr, "Request failed: %s\n", e.what());
});
```

## Move semantics

The values in the future chain don't need to be copyable, the values are moved
from a continuation to the next:

```c++
std::unique_ptr<int> p = Pledge::via(&threadPool, [] {
  return std::make_unique<int>(1);
}).then([] (std::unique_ptr<int> p) {
  ++*p;
  return p;
}).get();
assert(*p == 2);
```

Since calling `.get()` or `.then()` will eventually move the value out from the
previous future, you can only call one of the functions per object. To enforce
this, you need to have an rvalue reference to call any of those functions.

```c++
Pledge::Future<int> future{1000};

// These wouldn't compile!
future.then([] (int v) { ... });
future.get();

// This is fine
std::move(future).then([] (int v) { ... }).get();
```

Promises and Futures themselves are movable but not copyable.

# Using this library

Pledge is a header-only library. One way of using it in your project is to add
it as a git submodule and just include it:

```sh
git submodule add https://github.com/tonttu/pledge.git
```

```c++
#include <pledge/Future.hpp>
```

# Motivation

Pledge was written as a simpler replacement to the
[Folly Futures](https://github.com/facebook/folly/blob/main/folly/docs/Futures.md)
library. Folly doesn't support void futures, instead you are expected to use a
dummy `folly::Unit` type instead. It's also non-trivial to extract the Futures
part of Folly without bringing in the rest of the huge library and its
dependencies.

On the other hand Folly Futures has lots of features Pledge doesn't, and it
probably wasn't written in a single weekend like Pledge, so it might be a bit
more mature.

# License

Pledge is released under MIT license.
