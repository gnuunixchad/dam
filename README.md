# <img src="./misc/dam-repo.png" width="24"/> My build of [dam](https://codeberg.org/sewn/dam)

dam is a itsy-bitsy dwm-esque bar for [river].

![](./dam-damblocks.png)

To use a status-bar, you can pass in status text via stdin:
```sh
# Start in river with damblocks, a line generator I wrote that can be found on
# https://codeberg.org/unixchad/damblocks
# https://github.com/gnuunixchad/damblocks
pgrep 'dam' || riverctl spawn "$HOME/.local/bin/damblocks | dam"
```

## Building

To build dam first ensure that you have the following dependencies:

* wayland
* wayland-protocols
* fcft
* pixman
* pkg-config

Afterwards, run:
```
make
make install
```

## Usage

Run `dam`.

To hide the current selected output's monitor, the `USR1` signal can be
sent to do so.

[river]: https://codeberg.org/river
