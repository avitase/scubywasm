import functools
import struct

import wasmtime


class WASMModule:
    def __init__(self, wasm, *, store):
        self._store = store
        self._instance = wasmtime.Instance(
            self._store,
            wasmtime.Module(self._store.engine, wasm),
            [],
        )

    def __getattr__(self, name):
        return functools.partial(self._instance.exports(self._store)[name], self._store)

    @property
    def store(self):
        return self._store

    def read_struct(self, fmt, ptr):
        memory = self._instance.exports(self._store)["memory"]
        return struct.unpack_from(fmt, memory.get_buffer_ptr(self._store), ptr)

    def write_struct(self, fmt, ptr, *values):
        memory = self._instance.exports(self._store)["memory"]
        memory.write(self._store, struct.pack(fmt, *values), ptr)
