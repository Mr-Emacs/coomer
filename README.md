# COOMER

> **Warning**
> This is meant for my use case hence why it might have some more stuff.
> This is a very basic install and a basic system. 

<img src="coomer.gif" width="300" alt="Alt text" />

This is tsoding boomer ported to C as nim is just mid of a language.

And it does not even build due to package manager of language issues as always
For more information please look into tsoding boomer.

## Dependency

You are required to have gcc, opengl-devel and glx-devel. Since I shipped glu as a vendor
dependency. I will in the future remove it.
To install on debian.

```sh
sudo apt install gcc libopengl-dev libgl-dev
```

## Build

The build process is relatively simple you just run make.
```sh
make
```

To install the `coomer` lechucola. Run make install as sudo. Or set a prefix path.

- Global Install
```sh
sudo make install # Defaults to /usr/local/bin
```

- Local Install
```sh
make install PREFIX=/home/urmom/.local/bin
```

