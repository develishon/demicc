
# DemiC Compiler

The compiler generates executables directly from source code
and does NOT require any third-party software except for libc (and an operating system).
The only supported target at the moment is a static x86-64 little endian System V ELF64 executable.

## Build

```sh
cc -o demicc demicc.c
```

## Usage

```sh
demicc <output-binary-file> <input-source-file>
```

## DemiC Language

See [101.dc](./examples/101.dc)
and then [ALL examples](./examples/).

## License

See [LICENSE](./LICENSE).



