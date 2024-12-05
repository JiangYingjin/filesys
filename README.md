# Unix-like File System

## Run

> 推荐使用 Docker 运行！

首次启动：

```bash
docker run -it --name fs csjiangyj/filesys
```

退出后重新启动：

```bash
docker start fs && docker exec -it fs fs
```

## Requirement

1. 在内存中分配 16MB 空间作为文件系统的存储空间（应该也可以存储到本地）。该空间被划分为块，块大小为 1KB。假设地址长度为 24 位，请设计虚拟地址（virtual address）结构。设计 inode 应包含哪些信息，要求 inode 应支持 10 个直接块地址（direct block addresses）和一个间接块地址。

2. 前几个块可用于存储 i 节点，第一个 i 节点可用于根目录 (/)。 (你可以随意设计结构，只要合理并在报告中解释清楚即可）

3. 使用随机字符串填充创建的文件。这意味着您只需指定文件大小（KB）和路径+名称。

4. 您的系统应支持以下命令：
   1. 系统启动时的欢迎信息，包括组信息（名称和 ID）。这也是您的 "版权 "要求。
   2. 创建文件：createFile 文件名 文件大小
      - 例如：createFile /dir1/myFile 10 (KB)
      - 如果 fileSiz > 最大文件大小，则打印出错误信息。
   3. 删除文件：deleteFile 文件名
      - 即：deleteFile /dir1/myFile
   4. 创建目录：createDir
      - 即：createDir /dir1/sub1（应支持嵌套目录）
   5. 删除目录：deleteDir
      - 即：deleteDir /dir1/sub1 （不允许删除当前工作目录）
   6. 更改当前工作目录：changeDir
      - 即：changeDir /dir2
   7. 列出当前工作目录下的所有文件和子目录：dir
      - 您还需要列出至少两个文件属性（如文件大小、创建时间等）。
   8. 复制文件：cp
      - 即：cp file1 file2
   9. 显示存储空间的使用情况：sum
      - 显示 16MB 空间的使用情况。您需要列出已使用和未使用的数据块。
   10. 打印文件内容：cat
       - 在终端上打印出文件内容，即： cat /dir1/file1
   11. 加载和退出：退出程序并释放所有占用的内存，但内存的内容应保存在磁盘上，以便重新加载。

> 在作业要求基础上额外支持二级间接地址、硬链接等。

## Demo

![](demo/GIF-00.gif)
![](demo/GIF-01.gif)
![](demo/GIF-02.gif)
![](demo/GIF-03.gif)
![](demo/GIF-04.gif)
![](demo/GIF-05.gif)
![](demo/GIF-06.gif)
![](demo/GIF-07.gif)

![](demo/Screenshot-00.jpg)
![](demo/Screenshot-01.jpg)
![](demo/Screenshot-02.jpg)
![](demo/Screenshot-03.jpg)
![](demo/Screenshot-04.jpg)
![](demo/Screenshot-05.jpg)
![](demo/Screenshot-06.jpg)
![](demo/Screenshot-07.jpg)
![](demo/Screenshot-08.jpg)
![](demo/Screenshot-09.jpg)
![](demo/Screenshot-10.jpg)
![](demo/Screenshot-11.jpg)
