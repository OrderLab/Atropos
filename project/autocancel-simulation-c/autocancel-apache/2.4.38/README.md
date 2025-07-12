# Apache

## Build

```bash
export PSANDBOXDIR=/DIR/TO/PSANDBOX/LIB
export LD_LIBRARY_PATH=/DIR/TO/PSANDBOX/LIB/BUILD
./configure --prefix=../apache2 --with-included-apr --enable-so --enable-cgi --with-mpm=prefork/worker/event
make
make install
```
