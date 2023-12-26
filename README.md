# Xilnx Virtual Cable sources for MilkV-Duo RISC-V system

Based on Xilinx XAPP1251 port for MilkV-Duo RISC-V [board](https://milkv.io/duo)

![image.png](image.png)
## How to build

1. Get toolchain and examples: git clone https://github.com/milkv-duo/duo-examples.git
1. Setup: source envsetup.sh
1. Build: Make
1. Copy executible file to the MilkV board by scp: make scp
1. start application and enjoy!
   
## How to build image and boot

See instruction here [https://milkv.io/docs/duo/getting-started/boot](https://milkv.io/docs/duo/getting-started/boot)

## Prototype at work
![xvc2c.jpeg](xvc2c.jpeg)
