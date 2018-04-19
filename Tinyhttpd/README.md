# tinyhttpd

**tinyhttpd** 是一个简单的Http Server，由纯C语言编写，涉及Socket、Http数据包、CGI等知识，有助于读者了解Web服务器的基本原理。

## 工作流程
![](https://github.com/zjd0112/classic_wheels/blob/master/Tinyhttpd/imgs/workflow.png)

## 代码改动
* 对原代码修改，使之在Ubuntu 16.04上可运行
* 将原来的 perl 语言编写的CGI脚本替换，用 Python 语言编写一个简单的演示脚本

## 编译
直接make编译即可  
`cd tinyhttpd`  
`make clean`  
`make`

## 使用
make生成的执行文件名称为 "httpd"  
执行 `./httpd`  
服务器默认在端口4000进行监听  
在浏览器输入`127.0.0.1:4000`即可

