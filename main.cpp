/*

写一个类似 ext2 的文件系统

要求：
1．在内存中分配 16MB 空间作为文件系统的存储空间（应该也可以存储到本地）。该空间被划分为块，块大小为 1KB。假设地址长度为 24 位，请设计虚拟地址（virtual address）结构。设计 inode 应包含哪些信息，要求 inode 应支持 10 个直接块地址（direct block addresses）和一个间接块地址。

2．前几个块可用于存储 i 节点，第一个 i 节点可用于根目录 (/)。 (你可以随意设计结构，只要合理并在报告中解释清楚即可）

3.使用随机字符串填充创建的文件。这意味着您只需指定文件大小（KB）和路径+名称。
4.您的系统应支持以下命令： a) 带有群组的欢迎信息。
a)系统启动时的欢迎信息，包括组信息（名称和 ID）。这也是您的 "版权 "要求。
b)创建文件：createFile 文件名 文件大小
例如：createFile /dir1/myFile 10 (KB)
如果 fileSiz > 最大文件大小，则打印出错误信息。
c)删除文件：deleteFile 文件名
即：deleteFile /dir1/myFile
d)创建目录：createDir
即：createDir /dir1/sub1（应支持嵌套目录）
e)删除目录：deleteDir
即：deleteDir /dir1/sub1 （不允许删除当前工作目录）
f)更改当前工作目录：changeDir
即：changeDir /dir2
g)列出当前工作目录下的所有文件和子目录：dir
您还需要列出至少两个文件属性（如文件大小、创建时间等）。
h)复制文件：cp
即：cp file1 file2
i)显示存储空间的使用情况：sum
显示 16MB 空间的使用情况。您需要列出已使用和未使用的数据块。
j)打印文件内容：cat
在终端上打印出文件内容
即： cat /dir1/file1
k)您不需要实现 Login 的功能！
5.加载和退出：退出程序并释放所有占用的内存，但内存的内容应保存在磁盘上，以便重新加载；

rm rm -r
touch mkdir
cd cp cat
sum
*/

#include <iostream>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
using namespace std;

#define FILESYSTEM_SIZE (16 * 1024 * 1024) // 16MB
#define BLOCK_SIZE (1024)
#define INODE_SIZE (64)
#define BLOCK_NUM (FILESYSTEM_SIZE / BLOCK_SIZE) // 16K
#define INODE_NUM (BLOCK_NUM / 2)                // 8K
#define MAX_FILE_SIZE ((NUM_DIRECT_BLOCK + NUM_INDIRECT_BLOCK * ADDRESS_PER_BLOCK + NUM_DOUBLE_INDIRECT_BLOCK * ADDRESS_PER_BLOCK * ADDRESS_PER_BLOCK) * BLOCK_SIZE)
// (10 + 512 + 512 * 512) KB = 262,666 KB
#define MAX_FILENAME_SIZE (30)

// 目录项 Directory Entry
#define DENTRY_SIZE (32)                                // 32 Byte
#define DENTRY_NUM_PER_BLOCK (BLOCK_SIZE / DENTRY_SIZE) // 32

// 组织结构
#define SUPERBLOCK_SIZE (1 * 1024)                // 1KB
#define BLOCK_BITMAP_SIZE (BLOCK_NUM / 8)         // 2KB
#define INODE_BITMAP_SIZE (INODE_NUM / 8)         // 1KB
#define INODE_TABLE_SIZE (INODE_NUM * INODE_SIZE) // 512KB

#define SUPERBLOCK_START (0)
#define BLOCK_BITMAP_START (SUPERBLOCK_START + SUPERBLOCK_SIZE)     // 1KB
#define INODE_BITMAP_START (BLOCK_BITMAP_START + BLOCK_BITMAP_SIZE) // 3KB
#define INODE_TABLE_START (INODE_BITMAP_START + INODE_BITMAP_SIZE)  // 4KB
#define BLOCK_START (INODE_TABLE_START + INODE_TABLE_SIZE)          // 516KB

// 地址长度
#define ADDRESS_SIZE (2)                              // 实际使用 14 位
#define ADDRESS_PER_BLOCK (BLOCK_SIZE / ADDRESS_SIZE) // 512
#define NUM_DIRECT_BLOCK (10)
#define NUM_INDIRECT_BLOCK (1)
#define NUM_DOUBLE_INDIRECT_BLOCK (1)

#define ROOT_INODE_ID (0)

class SuperBlock
{
public:
    int filesystem_size = FILESYSTEM_SIZE;                                  // 文件系统大小（Byte）
    int block_size = BLOCK_SIZE;                                            // 块（block）的大小（Byte）
    int block_num = BLOCK_NUM;                                              // 块（block）的数量
    int inode_size = INODE_SIZE;                                            // INode 的大小（Byte）
    int inode_num = INODE_NUM;                                              // INode 的数量
    int available_block_num = (FILESYSTEM_SIZE - BLOCK_START) / BLOCK_SIZE; // 可用块的数量
    int available_inode_num = INODE_NUM;                                    // 可用 INode 的数量
};

// 定义 inode 结构
class INode
{

public:
    short id;            // INode 编号，占用 2 Byte
    char file_type;      // 文件类型，占用 1 Byte
    short file_size;     // 文件大小，占用 2 Byte
    int32_t create_time; // 创建时间，占用 4 Byte
    int32_t modify_time; // 修改时间，占用 4 Byte
    short link_cnt;      // 链接数，占用 2 Byte
    // short file_cnt;                                             // 文件数，占用 2 Byte
    //                                                             // 仅当 file_type 为 d 时时有效
    short direct_block[NUM_DIRECT_BLOCK];                   // 直接地址，占用 20 Byte
    short indirect_block[NUM_INDIRECT_BLOCK];               // 间接地址，占用 2 Byte
    short double_indirect_block[NUM_DOUBLE_INDIRECT_BLOCK]; // 双重间接地址，占用 2 Byte
                                                            // 总共 39 Byte

    INode()
    {
        this->id = -1;
        this->file_type = 0;
        this->file_size = 0;
        this->create_time = 0;
        this->modify_time = 0;
        this->link_cnt = 0;
        clear_address();
    }

    void clear_address()
    {
        memset(direct_block, -1, sizeof(direct_block));
        memset(indirect_block, -1, sizeof(indirect_block));
        memset(double_indirect_block, -1, sizeof(double_indirect_block));
    }

    // 由直接块ID、间接块ID、双重间接块ID获取块ID向量
    vector<int> get_block_list()
    {
        vector<int> block_id;

        // 直接块
        for (int i = 0; i < NUM_DIRECT_BLOCK; i++)
        {
            // 判断是否为-1
            if (direct_block[i] != -1)
            {
                block_id.push_back(direct_block[i]);
            }
        }

        // 间接块
        for (int i = 0; i < NUM_INDIRECT_BLOCK; i++)
        {
            if (indirect_block[i] != -1)
            {
                short *block = new short[ADDRESS_PER_BLOCK];
                fseek(fp, BLOCK_START + indirect_block[i] * BLOCK_SIZE, SEEK_SET);
                fread(block, BLOCK_SIZE, 1, fp);
                for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                {
                    short id = block[j];
                    if (id != -1)
                    {
                        block_id.push_back(id);
                    }
                }
            }
        }

        // 双重间接块
        for (int i = 0; i < NUM_DOUBLE_INDIRECT_BLOCK; i++)
        {
            if (double_indirect_block[i] != -1)
            {
                short *block = new short[ADDRESS_PER_BLOCK];
                fseek(fp, BLOCK_START + double_indirect_block[i] * BLOCK_SIZE, SEEK_SET);
                fread(block, BLOCK_SIZE, 1, fp);
                for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                {
                    short id = block[j];
                    if (id != -1)
                    {
                        short *block2 = new short[ADDRESS_PER_BLOCK];
                        fseek(fp, BLOCK_START + id * BLOCK_SIZE, SEEK_SET);
                        fread(block2, BLOCK_SIZE, 1, fp);
                        for (int k = 0; k < ADDRESS_PER_BLOCK; k++)
                        {
                            short id2 = block2[k];
                            if (id2 != -1)
                            {
                                block_id.push_back(id2);
                            }
                        }
                        delete[] block2;
                    }
                }
                delete[] block;
            }
        }

        return block_id;
    }

    // 由块ID向量生成直接块ID、间接块ID、双重间接块ID
    // 确保文件大小不超过最大值才执行以下函数
    void set_block_list(vector<short> block_id)
    {

        // 清空原有数据
        clear_address();

        // 将 block_id 分成直接块、间接块、双重间接块三个部分
        vector<short> _direct_block_list;
        vector<short> _indirect_block_list;
        vector<short> _double_indirect_block_list;

        for (int i = 0; i < block_id.size(); i++)
        {
            if (i < NUM_DIRECT_BLOCK)
            {
                _direct_block_list.push_back(block_id[i]);
            }
            else if (i < NUM_DIRECT_BLOCK + NUM_INDIRECT_BLOCK * ADDRESS_PER_BLOCK)
            {
                _indirect_block_list.push_back(block_id[i]);
            }
            else if (i < NUM_DIRECT_BLOCK + NUM_INDIRECT_BLOCK * ADDRESS_PER_BLOCK + NUM_DOUBLE_INDIRECT_BLOCK * ADDRESS_PER_BLOCK * ADDRESS_PER_BLOCK)
            {
                _double_indirect_block_list.push_back(block_id[i]);
            }
        }

        if (!_direct_block_list.empty())
        {
            for (int i = 0; i < _direct_block_list.size(); i++)
            {
                direct_block[i] = _direct_block_list[i];
            }
        }

        if (!_indirect_block_list.empty())
        {
            for (int i = 0; i < NUM_INDIRECT_BLOCK; i++)
            {
                indirect_block[i] = get_available_block();
                short *block = new short[ADDRESS_PER_BLOCK];
                memset(block, -1, BLOCK_SIZE);

                for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                {
                    block[j] = _indirect_block_list[i * ADDRESS_PER_BLOCK + j];
                }

                fseek(fp, BLOCK_START + indirect_block[i] * BLOCK_SIZE, SEEK_SET);
                fwrite(block, BLOCK_SIZE, 1, fp);
                delete[] block;
            }
        }

        if (!_double_indirect_block_list.empty())
        {
            for (int i = 0; i < NUM_DOUBLE_INDIRECT_BLOCK; i++)
            {
                double_indirect_block[i] = get_available_block();
                short *block = new short[ADDRESS_PER_BLOCK];
                memset(block, -1, BLOCK_SIZE);

                for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                {
                    block[j] = get_available_block();
                    short *block2 = new short[ADDRESS_PER_BLOCK];
                    memset(block2, -1, BLOCK_SIZE);

                    for (int k = 0; k < ADDRESS_PER_BLOCK; k++)
                    {
                        block2[k] = _double_indirect_block_list[i * ADDRESS_PER_BLOCK * ADDRESS_PER_BLOCK + j * ADDRESS_PER_BLOCK + k];
                    }

                    fseek(fp, BLOCK_START + block[j] * BLOCK_SIZE, SEEK_SET);
                    fwrite(block2, BLOCK_SIZE, 1, fp);
                    delete[] block2;
                }

                fseek(fp, BLOCK_START + double_indirect_block[i] * BLOCK_SIZE, SEEK_SET);
                fwrite(block, BLOCK_SIZE, 1, fp);
                delete[] block;
            }
        }
    }

    // 只存放间接地址块，有别于数据块
    vector<short> *get_indirect_addr_block_list()
    {
        vector<short> *indirect_block_list;
        // 间接块
        for (int i = 0; i < NUM_INDIRECT_BLOCK; i++)
        {
            if (indirect_block[i] != -1)
            {
                indirect_block_list->push_back(indirect_block[i]);
            }
        }
        // 二级间接块
        for (int i = 0; i < NUM_DOUBLE_INDIRECT_BLOCK; i++)
        {
            if (double_indirect_block[i] != -1)
            {
                // 将二级中的第一级间接块加入列表
                indirect_block_list->push_back(double_indirect_block[i]);

                // 读取二级间接块
                Dentry *dentry = new Dentry[DENTRY_NUM_PER_BLOCK];
                fseek(fp, BLOCK_START + double_indirect_block[i] * BLOCK_SIZE, SEEK_SET);
                fread(dentry, BLOCK_SIZE, 1, fp);
                for (int j = 0; j < DENTRY_NUM_PER_BLOCK; j++)
                {
                    if (dentry[j].inode_id != -1)
                    {
                        // 将二级中的第二级数据块加入列表
                        indirect_block_list->push_back(dentry[j].inode_id);
                    }
                }
            }
        }
        return indirect_block_list;
    }

    auto load_data()
    {

        printf("读取 Inode 中 ...\n");
        printf("Inode ID: %d\n", id);
        printf("文件类型: %c\n", file_type);
        printf("文件大小: %d\n", file_size);
        printf("创建时间: %s", time_to_string(create_time));
        printf("修改时间: %s", time_to_string(modify_time));
        printf("链接数: %d\n", link_cnt);

        // 如果是目录
        if (file_type == 'd')
        {
            vector<short> block_id = get_block_list();
            vector<Dentry> dentry_list;
            // 读出所有块的数据并判断 Dentry 是否存在
            for (int i = 0; i < block_id.size(); i++)
            {
                Dentry *dentry = new Dentry[DENTRY_NUM_PER_BLOCK];
                fseek(fp, BLOCK_START + block_id[i] * BLOCK_SIZE, SEEK_SET);
                fread(dentry, BLOCK_SIZE, 1, fp);
                for (int j = 0; j < DENTRY_NUM_PER_BLOCK; j++)
                {
                    if (dentry[j].inode_id != -1)
                    {
                        printf("读取到 %s（inode_id: %d）\n", dentry[j].filename, dentry[j].inode_id);
                        dentry_list.push_back(dentry[j]);
                    }
                }
                delete[] dentry;
            }

            return dentry_list;
        }

        if (file_type == 'f')
        {
            vector<short> block_id = get_block_list();
            char *file_data = new char[file_size];
            for (int i = 0; i < block_id.size(); i++)
            {
                fseek(fp, BLOCK_START + block_id[i] * BLOCK_SIZE, SEEK_SET);
                fread(file_data + i * BLOCK_SIZE, BLOCK_SIZE, 1, fp);
            }
            printf("读取到文件内容：\n%s\n", file_data);
            return file_data;
        }
    }

    int add_dentry(const char *filename, short inode_id)
    {
        printf("正在添加目录项 %s（inode_id: %d）...\n", filename, inode_id);

        // 如果不是目录
        if (file_type != 'd')
        {
            printf("id 为 %d 的 inode 不是目录\n", id);
            return -1;
        }

        // 获取当前目录的数据块
        vector<short> block_id = get_block_list();
        // 遍历数据块，找到第一个空闲的 Dentry 位置
        for (int i = 0; i < block_id.size(); i++)
        {
            Dentry *dentry = new Dentry[DENTRY_NUM_PER_BLOCK];
            fseek(fp, BLOCK_START + block_id[i] * BLOCK_SIZE, SEEK_SET);
            fread(dentry, BLOCK_SIZE, 1, fp);
            for (int j = 0; j < DENTRY_NUM_PER_BLOCK; j++)
            {
                if (dentry[j].inode_id == -1)
                {
                    dentry[j].inode_id = inode_id;
                    strcpy(dentry[j].filename, filename);
                    fseek(fp, BLOCK_START + block_id[i] * BLOCK_SIZE, SEEK_SET);
                    fwrite(dentry, BLOCK_SIZE, 1, fp);
                    delete[] dentry;
                    return 0;
                }
            }
            delete[] dentry;
        }
    };

    int delete_dentry(const char *filename)
    {
        printf("正在删除目录项 %s ...\n", filename);

        // 如果不是目录
        if (file_type != 'd')
        {
            printf("id 为 %d 的 inode 不是目录\n", id);
            return -1;
        }

        // 获取当前目录的数据块
        vector<short> block_id = get_block_list();
        // 遍历数据块，找到对应的 Dentry 并删除
        for (int i = 0; i < block_id.size(); i++)
        {
            Dentry *dentry = new Dentry[DENTRY_NUM_PER_BLOCK];
            fseek(fp, BLOCK_START + block_id[i] * BLOCK_SIZE, SEEK_SET);
            fread(dentry, BLOCK_SIZE, 1, fp);
            for (int j = 0; j < DENTRY_NUM_PER_BLOCK; j++)
            {
                if (strcmp(dentry[j].filename, filename) == 0)
                {
                    dentry[j].inode_id = -1;
                    memset(dentry[j].filename, 0, MAX_FILENAME_SIZE);
                    fseek(fp, BLOCK_START + block_id[i] * BLOCK_SIZE, SEEK_SET);
                    fwrite(dentry, BLOCK_SIZE, 1, fp);
                    delete[] dentry;
                    return 0;
                }
            }
            delete[] dentry;
        }
    };

    // 递归获取一个目录下的所有 inode id（包括该目录）
    vector<short> get_all_inode_id()
    {
        if (file_type != 'd')
        {
            // 返回自己的 inode_id
            return {id};
        }

        else if (file_type == 'd')
        {
            vector<short> inode_id_list = {id};
            vector<Dentry> dentry_list = load_data();
            for (auto dentry : dentry_list)
            {
                short inode_id = dentry.inode_id;
                INode inode = inode_table[inode_id];
                vector<short> sub_inode_id_list = inode.get_all_inode_id();
                inode_id_list.insert(inode_id_list.end(), sub_inode_id_list.begin(), sub_inode_id_list.end());
            }
            return inode_id_list;
        }
    }

}

class Dentry
{
public:
    short inode_id;
    char filename[MAX_FILENAME_SIZE];

public:
    Dentry()
    {
        inode_id = -1;
        memset(filename, 0, MAX_FILENAME_SIZE);
    }
    ~Dentry();
};

class Bitmap
{
public:
    char *bitmap;

    Bitmap(int num)
    {
        // 由于 num 不能被 8 整除，所以加 1
        int size = num / 8 + 1;
        bitmap = new char[size];
        memset(bitmap, 0, size);
    }

    void set(int pos, bool flag = 1)
    {
        int byte_pos = pos / 8;
        int bit_pos = pos % 8;
        if (flag)
        {
            bitmap[byte_pos] |= (1 << bit_pos);
        }
        else
        {
            bitmap[byte_pos] &= ~(1 << bit_pos);
        }
    }
};

// 定义文件系统结构
class FileSystem
{
    // public:
    //     static const int BLOCK_SIZE = 1024;
    //     static const int BLOCK_NUM = 16 * 1024 * 1024 / BLOCK_SIZE;
    //     static const int MAX_FILENAME_SIZE = 20;

public:
    FileSystem()
    {

        // superblock = SuperBlock();
        // inode_table = new INode[superblock.inode_num];
        // block_bitmap = new char[superblock.block_num / 8];
        // inode_bitmap = new char[superblock.inode_num / 8];
        // block = new char[superblock.filesystem_size - BLOCK_START];
        // working_dir = new char[1024];
        // memset(block_bitmap, 0, superblock.block_num / 8);
        // memset(inode_bitmap, 0, superblock.inode_num / 8);
        // memset(block, 0, superblock.filesystem_size - BLOCK_START);
        // strcpy(working_dir, "/");
    }

    void createFile(const char *filename, int filesize);
    void deleteFile(const char *filename);
    void createDir(const char *dirname);
    void deleteDir(const char *dirname);
    void changeDir(const char *dirname);
    void dir();
    void cp(const char *src, const char *dst);
    void sum();
    void cat(const char *filename);

    // const char *get_parent_path(const char *path)
    // {
    //     // 需要考虑没有 / 的情况，此时父目录为当前工作目录
    //     for (int i = strlen(path) - 1; i >= 0; i--)
    //     {
    //         if (path[i] == '/')
    //         {
    //             char *parent_path = new char[i + 1];
    //             strncpy(parent_path, path, i);
    //             parent_path[i] = '\0';
    //             return parent_path;
    //         }
    //     }
    //     return working_dir;
    // }

    // short path_to_inode(const char *path)
    // {
    //     // 从根目录开始
    //     short cur_inode_id = ROOT_INODE_ID;
    //     // 从根目录开始
    //     INode cur_inode = inode_table[cur_inode_id];
    //     // 分割路径
    //     char *path_copy = new char[strlen(path) + 1];
    //     strcpy(path_copy, path);
    //     char *token = strtok(path_copy, "/");
    //     while (token != NULL)
    //     {
    //         // 获取当前目录的数据块
    //         vector<Dentry> dentry_list = cur_inode.load_data();
    //         // 遍历目录项
    //         bool found = false;
    //         for (int i = 0; i < dentry_list.size(); i++)
    //         {
    //             if (strcmp(dentry_list[i].filename, token) == 0)
    //             {
    //                 cur_inode_id = dentry_list[i].inode_id;
    //                 cur_inode = inode_table[cur_inode_id];
    //                 found = true;
    //                 break;
    //             }
    //         }
    //         if (!found)
    //         {
    //             printf("路径不存在\n");
    //             return -1;
    //         }
    //         token = strtok(NULL, "/");
    //     }
    //     delete[] path_copy;
    //     return cur_inode_id;
    // }

    const char *relative_to_absolute(const char *path)
    {
        // 如果是绝对路径，直接返回
        if (path[0] == '/')
        {
            return path;
        }

        // 如果是相对路径，拼接当前工作目录
        char *absolute_path = new char[strlen(working_dir) + strlen(path) + 2];
        strcpy(absolute_path, working_dir);
        // 如果 working_dir 末尾没有 /，则添加一个
        if (working_dir[strlen(working_dir) - 1] != '/')
        {
            strcat(absolute_path, "/");
        }
        strcat(absolute_path, path);
        return absolute_path;
    }

    // 将路径分割为路径向量
    const char *get_path_vector(const char *path)
    {
        // 打印 path 及其字符串长度
        printf("path: %s\n", path);
        printf("strlen(path): %d\n", strlen(path));

        // 将路径转为绝对路径
        const char *absolute_path = relative_to_absolute(path);

        // 打印 absolute_path 及其字符串长度
        printf("absolute_path: %s\n", absolute_path);
        printf("strlen(absolute_path): %d\n", strlen(absolute_path));

        // 分割路径
        char *path_copy = new char[strlen(absolute_path) + 1];
        strcpy(path_copy, absolute_path);
        char *token = strtok(path_copy, "/");
        vector<string> dir_vector;
        while (token != NULL)
        {
            dir_vector.push_back(token);
            token = strtok(NULL, "/");
        }

        // 打印 dir_vector
        for (int i = 0; i < dir_vector.size(); i++)
        {
            printf("dir_vector[%d]: %s\n", i, dir_vector[i].c_str());
        }
        delete[] path_copy;
        return dir_vector;
    }

    void find_inode(const char *path, short &dir_inode_id, short &file_inode_id)
    {
        // 将路径转为路径向量
        vector<string> dir_vector = get_path_vector(path);

        // 将 dir_inode_id 初始化为 ROOT_INODE_ID
        dir_inode_id = ROOT_INODE_ID;

        // 将 file_inode_id 初始化为 -1
        file_inode_id = -1;

        for (int i = 0; i < dir_vector.size(); i++)
        {
            char *next_fname = dir_vector[i].c_str();

            // 获取当前目录的数据块
            INode dir_inode = inode_table[dir_inode_id];
            vector<Dentry> dentry_list = dir_inode.load_data();

            // 遍历目录项
            bool found = false;
            for (auto dentry : dentry_list)
            {
                if (strcmp(dentry.filename, next_fname) == 0)
                {
                    // 如果 i == dir_vector.size() - 1，则找到了文件
                    if (i == dir_vector.size() - 1)
                    {
                        file_inode_id = dentry.inode_id;
                    }
                    else
                    {
                        // 需要继续往下找
                        dir_inode_id = dentry.inode_id;
                    }
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                // 如果是最后一个，则说明文件不存在，否则说明目录不存在
                if (i == dir_vector.size() - 1)
                {
                    printf("文件 %s 不存在\n", next_fname);
                }
                else
                {
                    // 将 dir_inode_id 设置为 -1
                    dir_inode_id = -1;
                    printf("目录 %s 不存在\n", next_fname);
                }
            }
        }

        // 打印 dir_inode_id 和 file_inode_id
        printf("dir_inode_id: %d\n", dir_inode_id);
        printf("file_inode_id: %d\n", file_inode_id);
    }

    void remove_file(const char *path, recursive = false)
    {

        // 将路径转为绝对路径
        const char *absolute_path = relative_to_absolute(path);

        // 寻找 path 的 inode
        short dir_inode_id, file_inode_id;
        find_inode(path, dir_inode_id, file_inode_id);

        // 如果 file_inode_id 为 -1，则说明文件不存在
        if (file_inode_id == -1)
        {
            printf("文件 %s 不存在\n", absolute_path);
            return;
        }

        // 获取目录和文件的 inode
        INode dir_inode = inode_table[dir_inode_id];
        INode file_inode = inode_table[file_inode_id];

        // 如果是目录，recursive 为 false，则不允许删除
        if (file_inode.file_type == 'd' && !recursive)
        {
            printf("文件 %s 是目录，不允许删除\n", absolute_path);
            return;
        }

        // 如果是文件
        if (file_inode.file_type == 'f')
        {
            // 删除所有数据块
            vector<short> block_id = file_inode.get_block_list();
            for (auto id : block_id)
            {
                delete_block(id);
            }

            // 删除所有间接地址块
            vector<short> *indirect_block_list = file_inode.get_indirect_addr_block_list();
            for (auto id : *indirect_block_list)
            {
                delete_block(id);
            }

            // 删除目录项
            dir_inode.delete_dentry(file_inode.filename);
            printf("文件 %s 删除成功\n", path);

            // 删除文件 inode
            delete_inode(file_inode_id);
        }

        // 如果是目录，并且 recursive 为 true
        if (file_inode.file_type == 'd' && recursive)
        {
            // 获取目录下所有 inode_id
            vector<short> inode_id_list = file_inode.get_all_inode_id();
            // 逐个删除
            for (auto id : inode_id_list)
            {
                INode inode = inode_table[id];
                if (inode.file_type == 'f')
                {
                    // 删除所有数据块
                    vector<short> block_id = inode.get_block_list();
                    for (auto id : block_id)
                    {
                        delete_block(id);
                    }

                    // 删除所有间接地址块
                    vector<short> *indirect_block_list = inode.get_indirect_addr_block_list();
                    for (auto id : *indirect_block_list)
                    {
                        delete_block(id);
                    }

                    // 删除目录项
                    dir_inode.delete_dentry(inode.filename);
                    printf("文件 %s 删除成功\n", path);

                    // 删除文件 inode
                    delete_inode(id);
                }
            }
            for (auto id : inode_id_list)
            {
                INode inode = inode_table[id];
                if (inode.file_type == 'd')
                {
                    // 删除目录项
                    dir_inode.delete_dentry(inode.filename);
                    printf("目录 %s 删除成功\n", path);

                    // 删除目录 inode
                    delete_inode(id);
                }
            }
        }
    }

private:
    FILE *fp;

    // 持久化数据
    SuperBlock superblock;
    Bitmap *block_bitmap;
    Bitmap *inode_bitmap;
    INode *inode_table;

    // 运行时数据
    char *working_dir;
    short working_dir_inode_id;

    int load()
    {
        // 尝试加载文件系统
        fp = fopen("file.sys", "rb");
        // 如果文件不存在，则创建文件系统
        if (fp == NULL)
        {
            printf("尝试创建新文件系统中 ...\n");

            // 新建文件
            fp = fopen("file.sys", "wb");
            if (fp == NULL)
            {
                printf("创建文件系统失败\n");
                return -1;
            }
            else
            {
                // 根据文件系统大小分配空间（将所有数据置为 0）
                char *data = new char[FILESYSTEM_SIZE];
                memset(data, 0, FILESYSTEM_SIZE);
                fwrite(data, FILESYSTEM_SIZE, 1, fp);
                delete[] data;

                // 创建新的文件系统（元数据）
                superblock = SuperBlock();
                block_bitmap = new Bitmap(superblock.block_num);
                inode_bitmap = new Bitmap(superblock.inode_num);
                inode_table = new INode[superblock.inode_num];

                // 保存到文件系统
                save_superblock();
                save_bitmap();

                init_root_dir();
                printf("创建文件系统成功\n");
                return 0;
            }
        }
        else
        {
            // 加载文件系统元数据
            load_superblock();
            load_bitmap();
            load_inode();
            return 0;
        }
    }

    void load_superblock()
    {
        fseek(fp, SUPERBLOCK_START, SEEK_SET);
        fread(&superblock, SUPERBLOCK_SIZE, 1, fp);
    }

    void save_superblock()
    {
        fseek(fp, SUPERBLOCK_START, SEEK_SET);
        fwrite(&superblock, SUPERBLOCK_SIZE, 1, fp);
    }

    void load_bitmap()
    {
        fseek(fp, BLOCK_BITMAP_START, SEEK_SET);
        fread(block_bitmap->bitmap, superblock.block_num / 8, 1, fp);

        fseek(fp, INODE_BITMAP_START, SEEK_SET);
        fread(inode_bitmap->bitmap, superblock.inode_num / 8, 1, fp);
    }

    void save_bitmap()
    {
        fseek(fp, BLOCK_BITMAP_START, SEEK_SET);
        fwrite(block_bitmap->bitmap, superblock.block_num / 8, 1, fp);

        fseek(fp, INODE_BITMAP_START, SEEK_SET);
        fwrite(inode_bitmap->bitmap, superblock.inode_num / 8, 1, fp);
    }

    void load_inode()
    {
        fseek(fp, INODE_TABLE_START, SEEK_SET);
        fread(inode_table, INODE_TABLE_SIZE, 1, fp);
    }

    void save_inode(int id)
    {
        fseek(fp, INODE_TABLE_START + id * INODE_SIZE, SEEK_SET);
        fwrite(&inode_table[id], INODE_SIZE, 1, fp);
    }

    // 获取可用块 ID 并在 bitmap 中标记已使用
    short get_available_block()
    {
        for (int i = 0; i < superblock.block_num; i++)
        {
            if (!block_bitmap->bitmap[i])
            {
                block_bitmap->set(i);
                return i;
            }
        }
        return -1;
    }

    // 获取可用 inode ID 并在 bitmap 中标记已使用
    short get_available_inode()
    {
        for (int i = 0; i < superblock.inode_num; i++)
        {
            if (!inode_bitmap->bitmap[i])
            {
                inode_bitmap->set(i);
                return i;
            }
        }
        return -1;
    }

    void delete_inode(short id)
    {
        inode_bitmap->set(id, 0);
        inode_table[id] = INode(); // 清空 inode 中的数据
        save_inode(id);
    }

    void delete_block(short id)
    {
        // 将对应的 bitmap 重置为 0
        block_bitmap->set(id, 0);
        // 清空文件中该 block 的数据
        char *block = new char[BLOCK_SIZE];
        memset(block, 0, BLOCK_SIZE);
        fseek(fp, BLOCK_START + id * BLOCK_SIZE, SEEK_SET);
        fwrite(block, BLOCK_SIZE, 1, fp);
        delete[] block;
    }

    // 初始化根目录
    void init_root_dir()
    {
        // INode
        INode &root_dir = inode_table[ROOT_INODE_ID];
        root_dir.id = ROOT_INODE_ID;
        root_dir.file_type = 'd';
        root_dir.file_size = BLOCK_SIZE;
        root_dir.create_time = get_current_time();
        root_dir.modify_time = get_current_time();
        root_dir.link_cnt = 0;
        root_dir.direct_block[0] = get_available_block();

        // Dentry
        Dentry root_dentry;
        root_dentry.inode_id = ROOT_INODE_ID;
        root_dentry.filename[0] = '/';
        // 将 Dentry 写入 block
        // 遍历 block，找到第一个空闲的 Dentry 位置
        for (int i = 0; i < DENTRY_NUM_PER_BLOCK; i++)
        {
            // 读取 block
            char *block = new char[BLOCK_SIZE];
            fseek(fp, BLOCK_START + root_dir.direct_block[0] * BLOCK_SIZE, SEEK_SET);
            fread(block, BLOCK_SIZE, 1, fp);
            // 找到空闲位置
            if (block[i * DENTRY_SIZE] == 0)
            {
                memcpy(block + i * DENTRY_SIZE, &root_dentry, DENTRY_SIZE);
                break;
            }
            delete[] block;
        }
        char *block = new char[BLOCK_SIZE];
        int dentry_num = 0;
        memcpy(block, &dentry_num, sizeof(int));
    }

public:
    // 获取当前时间
    int32_t get_current_time()
    {
        auto now = std::chrono::system_clock::now();
        auto now_seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
        return now_seconds.time_since_epoch().count();
    }

    // 将时间戳转换为字符串
    char *time_to_string(int32_t timestamp)
    {
        time_t t = timestamp;
        struct tm *tm = localtime(&t);
        return asctime(tm);
    }
};

int main(int argc, char *argv[])
{
    // 打印 INode 大小
    printf("INode size: %d\n", sizeof(INode)); // 48
    // 打印 File 大小
    printf("Dentry size: %d\n", sizeof(Dentry)); // 32
    FileSystem fs;

    // // 打印 block_bitmap 的大小
    // printf("Block bitmap size: %d\n", BLOCK_BITMAP_SIZE); // 2048
    // // 打印 inode_bitmap 的大小
    // printf("INode bitmap size: %d\n", INODE_BITMAP_SIZE); // 1024
    // // 打印时间测试
    // cout << fs.time_to_string(fs.get_current_time()) << endl;

    return 0;
}
