# 使用基础的 Ubuntu 镜像
FROM ubuntu:latest

# 更新软件包列表并安装编译工具和依赖
# RUN apt update && \
#     apt install -y build-essential

# RUN apt install -y xmake

# RUN command -v wget || apt install -y wget

# RUN wget file.jiangyj.tech/proj/filesys/include.tar && \
#     tar -xvf include.tar && \
#     mv include /usr/local

# # 将本地 C++ 程序文件复制到镜像中
# COPY src /filesys/src
# COPY xmake.lua /filesys/xmake.lua

COPY build/linux/x86_64/release/main .

# # 设置工作目录
# WORKDIR /filesys

# # 编译 C++ 程序
# RUN xmake --root

# # 指定容器启动时执行的命令
# CMD ["/filesys/build/linux/x86_64/release/main"]
CMD ["./main"]