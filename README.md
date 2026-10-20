# Cutefish Installer

Installs Cutefish from the live system onto a disk. It opens full screen when
the live session starts, with the choice to try the system first; "Install
Cutefish" in the launcher brings it back.

The installer erases the chosen disk, copies the live image onto it, creates
the first user and installs the boot loader. The work is done as root by
`cutefish-installer-service`, a D-Bus activated system service, which only
runs from a live medium.

## Dependencies

The installer uses cutefish-toolkit; GoogleTest for the tests. At run time
the service needs util-linux, fdisk, e2fsprogs, dosfstools and squashfs-tools.

```sh
sudo apt install cmake pkg-config libgtest-dev
```

## Build and install

```sh
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```

## Tests

```sh
cmake -DBUILD_TESTING=ON ..
make
ctest --output-on-failure
```

## License

BSD-3-Clause; see [LICENSE](LICENSE).
