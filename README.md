# linux_apps_btc08_test
Test application for BTC08 chip.

How to Build for ASIC
#> source /opt/poky/2.5.1_64/environment-setup-aarch64-poky-linux
#> ./autogen.sh --host=aarch64-poky-linux-gnueabi
#> make

How to Build for FPGA
#> source /opt/poky/2.5.1_64/environment-setup-aarch64-poky-linux
#> ./autogen.sh --host=aarch64-poky-linux-gnueabi --enable-btc08-fpga
#> make
