# 机动模型示例目录结构说明


1.使用说明：机动测试模型及测试程序编译。

目前仅支持windows、linux。

# 1.使用说明

## 1.1 windows环境配置

1）下载SDK开发包，解压到与sim-cpp-move-test-model同级目录

2）使用Qt5.14.2版本，编译器使用MinGW 7.3.0 64-bit，打开sim-cpp-move-test-model.pro文件。

3) debug版本目录：bin\winDebug

4）release版本目录：bin\winRelease

5）将SDK\SDK\SimModel\Cpp\bin\winRelease（或winDebug）\SimSdk.dll放到 3）或 4）目录，在cmd中运行测试程序。

## 1.2 linux环境配置：

1）Centos 7    gcc 4.8.5   cmake 2.8.12.2

2）运行moveBuild.sh脚本编译测试模型及测试程序

3) debug版本目录：bin\unixDebug

4）release版本目录：bin\unixRelease

5）将SDK\SDK\SimModel\Cpp\bin\unixRelease（或unixDebug）\libSimSdk.dll放到 3）或 4）目录，在终端运行测试程序。

