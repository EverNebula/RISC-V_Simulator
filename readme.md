# RISC-V Simulator

## 概述
RISC-V Simulator实现了对于RISC-V架构的用户程序的简单功能模拟与性能模拟，并实现了单周期与五级流水线两种形式的模拟器。对于流水线模拟器，实现了多种分支跳转预测以及处理器运行周期的配置，并给之后模拟器的扩展提供了接口。

## 编译运行
模拟器的开发基于`RISC-V toolchain`，同时在代码中使用了`boost`库。进入项目文件夹后可以使用`make`来构建相应的目标：

- `pipeline` : 流水线模拟器
- `single` : 单周期模拟器
- `libecall` : 模拟器专用系统调用库（测试程序依赖）
- `testprog` : 所有的测试程序
- `all` : 将会自动编译流水线模拟器以及测试程序

之后运行`./sim`运行模拟器，参数如下：

- `-f <filename>` : __必要参数__，指定RISC-V用户程序路径
- `-s` : 单步调试
- `-v` : 输出指令相关信息
- `-d` : 输出全部debug信息
- `-p <type>` : 分支预测策略，目前支持五种，`type`为`0-4`，具体在后文解释，也可使用`-h`查看
- `-c <filename>` : 指令执行周期配置文件，格式参照`cfg/default.cfg`
- `-h` : 帮助信息

单步调试指令：

- `c` : 执行下一个周期/指令
- `r` : 输出所有寄存器
- `d` : 输出当前模拟器的所有信息到文件`status_dump.txt`
- `p` : 输出当前页表信息
- `m <address/hex> <size/dec>`: 查看`address`处`size`大小的数据，需要数据对齐
- `q` : 退出模拟器
