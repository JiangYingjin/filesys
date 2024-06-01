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

rm
rm -r
touch   =
mkdir
cd
cp
cat
sum
*/

#include <iostream>
#include <fstream>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <cmath>

#include "Log.cpp"

using namespace std;

#define FILESYSTEM_NAME "file.sys"
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

#define DATA_BLOCK_NUM ((FILESYSTEM_SIZE - BLOCK_START) / BLOCK_SIZE)

// 地址长度
#define ADDRESS_SIZE (2)                              // 实际使用 14 位
#define ADDRESS_PER_BLOCK (BLOCK_SIZE / ADDRESS_SIZE) // 512
#define NUM_DIRECT_BLOCK (10)
#define NUM_INDIRECT_BLOCK (1)
#define NUM_DOUBLE_INDIRECT_BLOCK (1)

#define ROOT_INODE_ID (0)

// 打印所有的宏定义
void print_all_macros()
{
    printf("FILESYSTEM_NAME: %s\n", FILESYSTEM_NAME);
    printf("FILESYSTEM_SIZE: %d\n", FILESYSTEM_SIZE);
    printf("BLOCK_SIZE: %d\n", BLOCK_SIZE);
    printf("INODE_SIZE: %d\n", INODE_SIZE);
    printf("BLOCK_NUM: %d\n", BLOCK_NUM);
    printf("INODE_NUM: %d\n", INODE_NUM);
    printf("MAX_FILE_SIZE: %d\n", MAX_FILE_SIZE);
    printf("MAX_FILENAME_SIZE: %d\n", MAX_FILENAME_SIZE);
    printf("DENTRY_SIZE: %d\n", DENTRY_SIZE);
    printf("DENTRY_NUM_PER_BLOCK: %d\n", DENTRY_NUM_PER_BLOCK);
    printf("SUPERBLOCK_SIZE: %d\n", SUPERBLOCK_SIZE);
    printf("BLOCK_BITMAP_SIZE: %d\n", BLOCK_BITMAP_SIZE);
    printf("INODE_BITMAP_SIZE: %d\n", INODE_BITMAP_SIZE);
    printf("INODE_TABLE_SIZE: %d\n", INODE_TABLE_SIZE);
    printf("SUPERBLOCK_START: %d\n", SUPERBLOCK_START);
    printf("BLOCK_BITMAP_START: %d\n", BLOCK_BITMAP_START);
    printf("INODE_BITMAP_START: %d\n", INODE_BITMAP_START);
    printf("INODE_TABLE_START: %d\n", INODE_TABLE_START);
    printf("BLOCK_START: %d\n", BLOCK_START);
    printf("DATA_BLOCK_NUM: %d\n", DATA_BLOCK_NUM);
    printf("ADDRESS_SIZE: %d\n", ADDRESS_SIZE);
    printf("ADDRESS_PER_BLOCK: %d\n", ADDRESS_PER_BLOCK);
    printf("NUM_DIRECT_BLOCK: %d\n", NUM_DIRECT_BLOCK);
    printf("NUM_INDIRECT_BLOCK: %d\n", NUM_INDIRECT_BLOCK);
    printf("NUM_DOUBLE_INDIRECT_BLOCK: %d\n", NUM_DOUBLE_INDIRECT_BLOCK);
    printf("ROOT_INODE_ID: %d\n", ROOT_INODE_ID);
}

class Util
{
public:
    // 获取当前时间
    static int32_t get_current_time()
    {
        auto now = chrono::system_clock::now();
        auto now_seconds = chrono::time_point_cast<chrono::seconds>(now);
        return now_seconds.time_since_epoch().count();
    }

    // 将时间戳转换为字符串
    static char *time_to_string(int32_t timestamp)
    {
        time_t t = timestamp;
        struct tm *tm = localtime(&t);
        return asctime(tm);
    }
};

class SuperBlock
{
public:
    int filesystem_size = FILESYSTEM_SIZE;                                  // 文件系统大小（Byte）
    int block_size = BLOCK_SIZE;                                            // 块（block）的大小（Byte）
    int block_num = BLOCK_NUM;                                              // 块（block）的数量
    int data_block_num = DATA_BLOCK_NUM;                                    // 数据块的数量
    int inode_size = INODE_SIZE;                                            // INode 的大小（Byte）
    int inode_num = INODE_NUM;                                              // INode 的数量
    int available_block_num = (FILESYSTEM_SIZE - BLOCK_START) / BLOCK_SIZE; // 可用块的数量
    int available_inode_num = INODE_NUM;                                    // 可用 INode 的数量

    SuperBlock()
    {
        // 打印文件系统信息
        cout << "文件系统大小：" << float(filesystem_size) / 1024 / 1024 << " MB" << endl;
        cout << "块（block）大小：" << block_size << " Byte" << endl;
        cout << "块（block）数量：" << block_num << endl;
        cout << "INode 大小：" << inode_size << " Byte" << endl;
        cout << "INode 数量：" << inode_num << endl;
        cout << "可用块数量：" << available_block_num << endl;
        cout << "可用 INode 数量：" << available_inode_num << endl;
    }
};

class Dentry
{
public:
    short inode_id;
    char filename[MAX_FILENAME_SIZE];

    Dentry()
    {
        inode_id = -1;
        set_filename("unknown");
    }

    Dentry(short inode_id, string filename)
    {
        this->inode_id = inode_id;
        set_filename(filename);
    }

    void set_filename(string filename)
    {
        if (filename.length() > MAX_FILENAME_SIZE)
        {
            cout << "文件名过长，filename.size()：" << filename.size() << "，最大长度：" << MAX_FILENAME_SIZE << endl;
            filename = filename.substr(0, MAX_FILENAME_SIZE);
            cout << "截断文件名存入：" << filename << endl;
        }
        strcpy(this->filename, filename.c_str());
    }

    string get_filename()
    {
        return string(filename);
    }
};

class INode
{
public:
    short id;                                               // INode 编号，占用 2 Byte
    char file_type;                                         // 文件类型，占用 1 Byte
    int file_size;                                          // 文件大小（单位 KB），占用 4 Byte
    int32_t create_time;                                    // 创建时间，占用 4 Byte
    int32_t modify_time;                                    // 修改时间，占用 4 Byte
    short link_cnt;                                         // 链接数，占用 2 Byte
    short direct_block[NUM_DIRECT_BLOCK];                   // 直接地址，占用 20 Byte
    short indirect_block[NUM_INDIRECT_BLOCK];               // 间接地址，占用 2 Byte
    short double_indirect_block[NUM_DOUBLE_INDIRECT_BLOCK]; // 双重间接地址，占用 2 Byte
                                                            // 总共 41 Byte
    INode()
    {
        this->id = -1;
        clear_address();
    }

    void clear_address()
    {
        memset(direct_block, -1, sizeof(direct_block));
        memset(indirect_block, -1, sizeof(indirect_block));
        memset(double_indirect_block, -1, sizeof(double_indirect_block));
    }

    friend ostream &operator<<(ostream &os, const INode &inode)
    {
        os << "inode.id: " << inode.id << endl;
        os << "inode.file_type: " << inode.file_type << endl;
        os << "inode.file_size: " << inode.file_size << endl;
        os << "inode.create_time: " << Util::time_to_string(inode.create_time) << endl;
        os << "inode.modify_time: " << Util::time_to_string(inode.modify_time) << endl;
        os << "inode.link_cnt: " << inode.link_cnt << endl;

        os << "inode.direct_block: ";
        for (int i = 0; i < NUM_DIRECT_BLOCK; i++)
            os << inode.direct_block[i] << " ";
        os << endl;

        os << "inode.indirect_block: ";
        for (int i = 0; i < NUM_INDIRECT_BLOCK; i++)
            os << inode.indirect_block[i] << " ";
        os << endl;

        os << "inode.double_indirect_block: ";
        for (int i = 0; i < NUM_DOUBLE_INDIRECT_BLOCK; i++)
            os << inode.double_indirect_block[i] << " ";
        os << endl;
        return os;
    }
};

class Bitmap
{
public:
    char *bitmap;

    Bitmap(int size_byte)
    {
        bitmap = new char[size_byte];
        memset(bitmap, 0, size_byte);
    }

    // 设置 bitmap 的特定 bit
    void set(int pos, bool flag = 1)
    {
        if (flag)
            bitmap[pos / 8] |= (1 << pos % 8);
        else
            bitmap[pos / 8] &= ~(1 << pos % 8);
    }

    bool get(int pos)
    {
        return bitmap[pos / 8] & (1 << pos % 8);
    }
};

class FileSystem
{

public:
    FILE *fp;

    // 持久化数据
    SuperBlock *superblock;
    Bitmap *block_bitmap;
    Bitmap *inode_bitmap;
    INode *inode_table;

    // 运行时数据
    string working_dir;
    short working_dir_inode_id;

    FileSystem()
    {
        if (!_is_filesys_exist())
        {
            cout << "文件系统不存在，创建中 ..." << endl;
            _create_filesys();
            cout << "文件系统创建成功" << endl;
        }
        else
        {
            cout << "文件系统存在，加载中 ..." << endl;
            _load(superblock, SUPERBLOCK_START, SUPERBLOCK_SIZE);
            _load(block_bitmap->bitmap, BLOCK_BITMAP_START, BLOCK_BITMAP_SIZE);
            _load(inode_bitmap->bitmap, INODE_BITMAP_START, INODE_BITMAP_SIZE);
            _load(inode_table, INODE_TABLE_START, INODE_TABLE_SIZE);
            cout << "文件系统加载成功" << endl;
        }
        _init_working_dir();
    }

    void _dump(void *data, int pos, int size)
    {
        fstream file(FILESYSTEM_NAME, ios::in | ios::out | ios::binary);
        if (!file)
            cout << "文件打开失败" << endl;
        file.seekp(pos, ios::beg);
        file.write((char *)data, size);
        file.close();
    }

    void _load(void *data, int pos, int size)
    {
        fstream file(FILESYSTEM_NAME, ios::in | ios::out | ios::binary);
        if (!file)
            cout << "文件打开失败" << endl;
        file.seekg(pos, ios::beg);
        file.read((char *)data, size);
        file.close();
    }

    // 创建文件系统
    void _create_filesys()
    {
        char *data = new char[FILESYSTEM_SIZE];
        memset(data, 0, FILESYSTEM_SIZE);
        fstream file(FILESYSTEM_NAME, ios::out | ios::binary | ios::trunc);
        file.write(data, FILESYSTEM_SIZE);
        file.close();

        superblock = new SuperBlock();
        block_bitmap = new Bitmap(BLOCK_BITMAP_SIZE);
        inode_bitmap = new Bitmap(INODE_BITMAP_SIZE);
        inode_table = new INode[superblock->inode_num];

        _dump(superblock, SUPERBLOCK_START, SUPERBLOCK_SIZE);
        _dump(block_bitmap->bitmap, BLOCK_BITMAP_START, BLOCK_BITMAP_SIZE);
        _dump(inode_bitmap->bitmap, INODE_BITMAP_START, INODE_BITMAP_SIZE);
        _dump(inode_table, INODE_TABLE_START, INODE_TABLE_SIZE);

        _init_root_dir();
    }

    // 初始化根目录
    void _init_root_dir()
    {
        cout << "初始化根目录 ..." << endl;

        // Inode
        INode &root_inode = inode_table[ROOT_INODE_ID];
        root_inode.id = ROOT_INODE_ID;
        root_inode.file_type = 'd';
        root_inode.file_size = BLOCK_SIZE;
        root_inode.create_time = Util::get_current_time();
        root_inode.modify_time = Util::get_current_time();
        root_inode.link_cnt = 0;
        root_inode.direct_block[0] = _get_avail_block();
        cout << root_inode << endl;
        _dump(&root_inode, INODE_TABLE_START + ROOT_INODE_ID * INODE_SIZE, INODE_SIZE);

        // 数据块
        _create_blank_dentries(root_inode.direct_block[0]);

        // Bitmap
        inode_bitmap->set(ROOT_INODE_ID);
        _save_bitmap();
        _show_bitmap();
    }

    void _init_working_dir()
    {
        working_dir = "/";
        working_dir_inode_id = ROOT_INODE_ID;
        cout << "working_dir: " << working_dir << endl;
        cout << "working_dir_inode_id: " << working_dir_inode_id << endl;
    }

    bool _is_filesys_exist()
    {
        ifstream file(FILESYSTEM_NAME);
        bool exist = file.good();
        file.close();
        return exist;
    }

    // 获取可用块 ID 并在 bitmap 中标记已使用
    short _get_avail_block()
    {
        for (int i = 0; i < superblock->block_num; i++)
            if (!block_bitmap->get(i))
            {
                cout << "可用块ID：" << i << endl;
                block_bitmap->set(i);
                _dump(block_bitmap->bitmap, BLOCK_BITMAP_START, BLOCK_BITMAP_SIZE);
                return i;
            }

        cout << "无可用块" << endl;
        return -1;
    }

    // 获取可用 inode ID 并在 bitmap 中标记已使用
    short _get_avail_inode()
    {
        for (int i = 0; i < superblock->inode_num; i++)
            if (!inode_bitmap->get(i))
            {
                cout << "可用 inode ID：" << i << endl;
                inode_bitmap->set(i);
                _dump(inode_bitmap->bitmap, INODE_BITMAP_START, INODE_BITMAP_SIZE);
                return i;
            }

        cout << "无可用 inode" << endl;
        return -1;
    }

    // 目录 d 的数据块
    void _create_blank_dentries(short block_id)
    {
        Dentry *dentry = new Dentry[DENTRY_NUM_PER_BLOCK];
        fill_n(dentry, DENTRY_NUM_PER_BLOCK, Dentry());
        _dump(dentry, BLOCK_START + block_id * BLOCK_SIZE, BLOCK_SIZE);
        delete[] dentry;
    }

    // inode 的间接地址数据块
    void _create_blank_address_block(short block_id)
    {
        short *block = new short[ADDRESS_PER_BLOCK];
        fill_n(block, ADDRESS_PER_BLOCK, -1);
        _dump(block, BLOCK_START + block_id * BLOCK_SIZE, BLOCK_SIZE);
        delete[] block;
    }

    // 由块ID向量生成直接块ID、间接块ID、双重间接块ID
    // 确保文件大小不超过最大值才执行以下函数
    void _set_block_list(INode &inode, vector<short> &block_id_list)
    {
        // 清空原有数据
        inode.clear_address();

        // 将 block_id_list 分成直接块、间接块、双重间接块三个部分
        vector<short> _direct_block_list;
        vector<short> _indirect_block_list;
        vector<short> _double_indirect_block_list;

        for (int i = 0; i < block_id_list.size(); i++)
        {
            cout << "block_id_list[" << i << "]: " << block_id_list[i] << endl;
            if (i < NUM_DIRECT_BLOCK)
                _direct_block_list.push_back(block_id_list[i]);
            else if (i < NUM_DIRECT_BLOCK + NUM_INDIRECT_BLOCK * ADDRESS_PER_BLOCK)
                _indirect_block_list.push_back(block_id_list[i]);
            else if (i < NUM_DIRECT_BLOCK + NUM_INDIRECT_BLOCK * ADDRESS_PER_BLOCK + NUM_DOUBLE_INDIRECT_BLOCK * ADDRESS_PER_BLOCK * ADDRESS_PER_BLOCK)
                _double_indirect_block_list.push_back(block_id_list[i]);
        }

        // 直接块
        if (!_direct_block_list.empty())
            for (int i = 0; i < _direct_block_list.size(); i++)
            {
                inode.direct_block[i] = _direct_block_list[i];
                cout << "_set_block_list: inode.direct_block[" << i << "]: " << inode.direct_block[i] << endl;
            }

        // 间接块
        if (!_indirect_block_list.empty())
        {
            inode.indirect_block[0] = _get_avail_block();

            short *block = new short[ADDRESS_PER_BLOCK];
            fill_n(block, ADDRESS_PER_BLOCK, -1);

            for (int j = 0; j < _indirect_block_list.size(); j++)
            {
                cout << "_set_block_list: _indirect_block_list[" << j << "]: " << _indirect_block_list[j] << endl;
                block[j] = _indirect_block_list[j];
            }

            _dump(block, BLOCK_START + inode.indirect_block[0] * BLOCK_SIZE, BLOCK_SIZE);
            delete[] block;
        }

        // 二级间接块
        if (!_double_indirect_block_list.empty())
        {
            cout << "_double_indirect_block_list.size(): " << _double_indirect_block_list.size() << endl;
            cout << "_set_block_list: ceil(float(_double_indirect_block_list.size()) / ADDRESS_PER_BLOCK): " << ceil(float(_double_indirect_block_list.size()) / ADDRESS_PER_BLOCK) << endl;

            inode.double_indirect_block[0] = _get_avail_block();
            short _1st_address_block[ADDRESS_PER_BLOCK]; // 二级地址块的地址
            fill_n(_1st_address_block, ADDRESS_PER_BLOCK, -1);

            for (int i = 0; i < ceil(float(_double_indirect_block_list.size()) / ADDRESS_PER_BLOCK); i++)
            {
                _1st_address_block[i] = _get_avail_block();
                cout << "_set_block_list: _1st_address_block[" << i << "]: " << _1st_address_block[i] << endl;

                short *block = new short[ADDRESS_PER_BLOCK];
                fill_n(block, ADDRESS_PER_BLOCK, -1);
                for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                {
                    if (i * ADDRESS_PER_BLOCK + j >= _double_indirect_block_list.size())
                        break;
                    block[j] = _double_indirect_block_list[i * ADDRESS_PER_BLOCK + j];
                    cout << "_set_block_list: _double_indirect_block_list[" << i * ADDRESS_PER_BLOCK + j << "]: " << block[j] << endl;
                }
                for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                    cout << "_set_block_list: test_block[" << j << "]: " << block[j] << endl;
                _dump(block, BLOCK_START + _1st_address_block[i] * BLOCK_SIZE, BLOCK_SIZE);
                delete[] block;
            }
            _dump(_1st_address_block, BLOCK_START + inode.double_indirect_block[0] * BLOCK_SIZE, BLOCK_SIZE);
        }

        _dump(&inode, INODE_TABLE_START + inode.id * INODE_SIZE, INODE_SIZE);
    }

    // 由 inode 的直接块ID、间接块ID、双重间接块ID获取其块ID向量
    // 返回块 ID 向量，根据这个向量就能获取所有内容
    vector<short> _get_block_list(INode &inode)
    {
        vector<short> block_id_list;

        // 直接块
        for (int i = 0; i < NUM_DIRECT_BLOCK; i++)
            if (inode.direct_block[i] != -1)
            {
                cout << "_get_block_list: inode.direct_block[i]: " << inode.direct_block[i] << endl;
                block_id_list.push_back(inode.direct_block[i]);
            }

        // 间接块
        for (int i = 0; i < NUM_INDIRECT_BLOCK; i++)
            if (inode.indirect_block[i] != -1)
            {
                cout << "_get_block_list: inode.indirect_block[" << i << "]: " << inode.indirect_block[i] << endl;
                short *block = new short[ADDRESS_PER_BLOCK];
                _load(block, BLOCK_START + inode.indirect_block[i] * BLOCK_SIZE, BLOCK_SIZE);
                for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                    if (block[j] != -1)
                    {
                        cout << "_get_block_list: inode.indirect_block[" << i << "], address[" << j << "]: " << block[j] << endl;
                        block_id_list.push_back(block[j]);
                    }
            }

        // 二级间接块
        for (int i = 0; i < NUM_DOUBLE_INDIRECT_BLOCK; i++)
            if (inode.double_indirect_block[i] != -1)
            {
                cout << "_get_block_list: inode.double_indirect_block[" << i << "]: " << inode.double_indirect_block[i] << endl;
                short *block = new short[ADDRESS_PER_BLOCK];
                _load(block, BLOCK_START + inode.double_indirect_block[i] * BLOCK_SIZE, BLOCK_SIZE);
                for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                    if (block[j] != -1)
                    {
                        cout << "_get_block_list: inode.double_indirect_block[" << i << "], address[" << j << "]: " << block[j] << endl;
                        short *block2 = new short[ADDRESS_PER_BLOCK];
                        _load(block2, BLOCK_START + block[j] * BLOCK_SIZE, BLOCK_SIZE);
                        for (int k = 0; k < ADDRESS_PER_BLOCK; k++)
                            if (block2[k] != -1)
                                block_id_list.push_back(block2[k]);
                        delete[] block2;
                    }
                delete[] block;
            }

        return block_id_list;
    }

    // 打印 bitmap
    void _show_bitmap()
    {
        Bitmap *block_bitmap_ = new Bitmap(BLOCK_BITMAP_SIZE);
        Bitmap *inode_bitmap_ = new Bitmap(INODE_BITMAP_SIZE);

        _load(block_bitmap_->bitmap, BLOCK_BITMAP_START, BLOCK_BITMAP_SIZE);
        _load(inode_bitmap_->bitmap, INODE_BITMAP_START, INODE_BITMAP_SIZE);

        cout << "Block Bitmap: " << endl;
        for (int i = 0; i < BLOCK_BITMAP_SIZE; i++)
            cout << (int)block_bitmap_->bitmap[i];
        cout << endl;

        cout << "INode Bitmap: " << endl;
        for (int i = 0; i < INODE_BITMAP_SIZE; i++)
            cout << (int)inode_bitmap_->bitmap[i];
        cout << endl;
    }

    // 保存 bitmap
    void _save_bitmap()
    {
        _dump(block_bitmap->bitmap, BLOCK_BITMAP_START, BLOCK_BITMAP_SIZE);
        _dump(inode_bitmap->bitmap, INODE_BITMAP_START, INODE_BITMAP_SIZE);
    }

    void _delete_inode(short id)
    {
        inode_table[id] = INode();
        _dump(&inode_table[id], INODE_TABLE_START + id * INODE_SIZE, INODE_SIZE);
        inode_bitmap->set(id, 0);
        _save_bitmap();
    }

    void _delete_block(short id)
    {
        char *block = new char[BLOCK_SIZE];
        memset(block, 0, BLOCK_SIZE);
        _dump(block, BLOCK_START + id * BLOCK_SIZE, BLOCK_SIZE);
        delete[] block;

        block_bitmap->set(id, 0);
        _save_bitmap();
    }

    // 只存放间接地址块，有别于数据块
    vector<short> *_get_indirect_addr_block_list(INode inode)
    {
        vector<short> *indirect_block_list;

        // 间接块
        for (int i = 0; i < NUM_INDIRECT_BLOCK; i++)
            if (inode.indirect_block[i] != -1)
                indirect_block_list->push_back(inode.indirect_block[i]);

        // 二级间接块
        for (int i = 0; i < NUM_DOUBLE_INDIRECT_BLOCK; i++)
            if (inode.double_indirect_block[i] != -1)
            {
                // 将二级中的第一级间接块加入列表
                indirect_block_list->push_back(inode.double_indirect_block[i]);

                // 读取二级间接块
                Dentry *dentry = new Dentry[DENTRY_NUM_PER_BLOCK];
                fseek(fp, BLOCK_START + inode.double_indirect_block[i] * BLOCK_SIZE, SEEK_SET);
                fread(dentry, BLOCK_SIZE, 1, fp);
                for (int j = 0; j < DENTRY_NUM_PER_BLOCK; j++)
                    if (dentry[j].inode_id != -1)
                        // 将二级中的第二级数据块加入列表
                        indirect_block_list->push_back(dentry[j].inode_id);
            }

        return indirect_block_list;
    }

    string _absolute_path(const string path)
    {
        // 打印 path
        cout << "path: " << path << endl;

        // 这里要检查 path 的有效性

        // 如果是绝对路径，直接返回
        if (path[0] == '/')
        {
            cout << "path 为绝对路径" << endl;
            return path;
        }

        // 如果是相对路径，拼接当前工作目录
        string absolute_path = working_dir;

        // 如果 working_dir 末尾没有 '/'，则添加一个
        if (!working_dir.empty() && working_dir.back() != '/')
        {
            absolute_path += "/";
        }

        absolute_path += path;
        return absolute_path;
    }

    // 将路径分割为路径向量
    vector<string> _get_path_vector(const string path)
    {
        // 打印 path 及其长度
        cout << "path: " << path << endl;
        cout << "path.length(): " << path.length() << endl;

        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        // 打印 absolute_path 及其长度
        cout << "absolute_path: " << absolute_path << endl;
        cout << "absolute_path.length(): " << absolute_path.length() << endl;

        // 分割路径
        istringstream iss(absolute_path);
        vector<string> dir_vector;
        string token;
        while (getline(iss, token, '/'))
            if (!token.empty())
                dir_vector.push_back(token);

        // 打印 dir_vector
        for (size_t i = 0; i < dir_vector.size(); i++)
            cout << "dir_vector[" << i << "]: " << dir_vector[i] << endl;

        return dir_vector;
    }

    string _get_filename(const string path)
    {
        // 将路径转为路径向量
        vector<string> dir_vector = _get_path_vector(path);
        cout << "filename: " << dir_vector.back() << endl;
        // 返回最后一个元素
        return dir_vector.back();
    }

    void _add_dentry(INode &dir_inode, const short &new_inode_id, string &filename)
    {
        cout << "正在添加目录项 " << filename << "（inode_id: " << new_inode_id << "）..." << endl;
        cout << "dir_inode.file_type: " << dir_inode.file_type << endl;

        // 如果不是目录
        if (dir_inode.file_type != 'd')
        {
            cout << "id 为 " << dir_inode.id << " 的 inode 不是目录" << endl;
            return;
        }

        // 获取当前目录的数据块
        vector<short> block_id_list = _get_block_list(dir_inode);
        cout << "block_id_list.size(): " << block_id_list.size() << endl;

        // 遍历数据块，找到第一个空闲的 Dentry 位置
        for (int i = 0; i < block_id_list.size(); i++)
        {
            cout << "block_id_list[" << i << "]: " << block_id_list[i] << endl;
            Dentry *dentry = new Dentry[DENTRY_NUM_PER_BLOCK];
            _load(dentry, BLOCK_START + block_id_list[i] * BLOCK_SIZE, BLOCK_SIZE);
            for (int j = 0; j < DENTRY_NUM_PER_BLOCK; j++)
            {
                cout << "block " << block_id_list[i] << ", dentry " << j << ", inode_id: " << dentry[j].inode_id << ", filename: " << dentry[j].filename << endl;
                if (dentry[j].inode_id == -1)
                {
                    dentry[j].inode_id = new_inode_id;
                    dentry[j].set_filename(filename);
                    cout << "block " << block_id_list[i] << ", dentry " << j << ", inode_id 被设置为 " << dentry[j].inode_id << ", filename 被设置为 " << dentry[j].filename << endl;
                    _dump(dentry, BLOCK_START + block_id_list[i] * BLOCK_SIZE, BLOCK_SIZE);
                    delete[] dentry;
                    return;
                }
            }
            delete[] dentry;
        }
        return;
    };

    void _add_dentry(short dir_inode_id, short new_inode_id, string &filename)
    {
        INode dir_inode = inode_table[dir_inode_id];
        _add_dentry(dir_inode, new_inode_id, filename);
    }

    vector<Dentry> _load_dentries(INode inode)
    {
        cout << "读取目录项中 ..." << endl;
        cout << "inode.id: " << inode.id << endl;
        cout << "inode.file_type: " << inode.file_type << endl;

        vector<Dentry> dentry_list;
        vector<short> block_id_list = _get_block_list(inode);
        cout << "block_id_list.size(): " << block_id_list.size() << endl;

        for (int i = 0; i < block_id_list.size(); i++)
        {
            cout << "block_id_list[" << i << "]: " << block_id_list[i] << endl;
            Dentry *dentry = new Dentry[DENTRY_NUM_PER_BLOCK];
            _load(dentry, BLOCK_START + block_id_list[i] * BLOCK_SIZE, BLOCK_SIZE);

            for (int j = 0; j < DENTRY_NUM_PER_BLOCK; j++)
            {
                cout << "dentry[" << j << "].inode_id: " << dentry[j].inode_id << endl;
                if (dentry[j].inode_id != -1)
                    dentry_list.push_back(dentry[j]);
            }
            delete[] dentry;
        }
        return dentry_list;
    }

    vector<Dentry> _load_dentries(short inode_id)
    {
        INode inode = inode_table[inode_id];
        return _load_dentries(inode);
    }

    vector<Dentry> _load_dentries(string &path)
    {
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);
        INode dir_inode = inode_table[dir_inode_id];
        return _load_dentries(dir_inode);
    }

    void _search_inode(const string path, short &dir_inode_id, short &file_inode_id)
    {
        cout << "根据路径 " << path << " 查找 inode_id ..." << endl;

        // 将路径转为路径向量
        vector<string> dir_vector = _get_path_vector(path);

        // 将 dir_inode_id 初始化为 ROOT_INODE_ID
        dir_inode_id = ROOT_INODE_ID;

        // 将 file_inode_id 初始化为 -1
        file_inode_id = -1;

        for (const auto &dir_name : dir_vector)
        {
            // 获取当前目录的数据块
            INode dir_inode = inode_table[dir_inode_id];
            vector<Dentry> dentry_list = _load_dentries(dir_inode);

            // 遍历目录项
            bool found = false;
            for (const auto &dentry : dentry_list)
                // 若两个字符串相等，则 strcmp 返回 0
                if (dentry.filename == dir_name)
                {
                    // 如果是最后一个目录项，则找到了文件
                    if (&dir_name == &dir_vector.back())
                        file_inode_id = dentry.inode_id;
                    else
                        // 需要继续往下找
                        dir_inode_id = dentry.inode_id;
                    found = true;
                    break;
                }

            if (!found)
                // 找到了次末级目录，但是没有找到最终项（文件/文件夹）
                // 如果是最后一个，则说明文件不存在，否则说明目录不存在
                if (&dir_name == &dir_vector.back())
                {
                    // 将 file_inode_id 设置为 -1
                    file_inode_id = -1;
                    // dir_inode_id 不变，说明找到了次末级目录
                    cout << "文件 " << dir_name << " 不存在" << endl;
                }
                else
                {
                    // 将 dir_inode_id 设置为 -1
                    dir_inode_id = -1;
                    // 将 file_inode_id 设置为 -1
                    file_inode_id = -1;
                    cout << "目录 " << dir_name << " 不存在" << endl;
                }
        }

        // 打印 dir_inode_id 和 file_inode_id
        cout << "dir_inode_id: " << dir_inode_id << endl;
        cout << "file_inode_id: " << file_inode_id << endl;
    }

    // 递归获取一个目录下的所有 inode id（包括该目录）
    vector<short> get_all_sub_inode_id(INode inode)
    {
        if (inode.file_type != 'd')
        {
            // 返回自己的 inode_id
            return {inode.id};
        }

        else if (inode.file_type == 'd')
        {
            vector<short> inode_id_list = {inode.id};
            vector<Dentry> dentry_list = _load_dentries(inode);
            for (auto dentry : dentry_list)
            {
                short inode_id = dentry.inode_id;
                INode _inode = inode_table[inode_id];
                vector<short> sub_inode_id_list = get_all_sub_inode_id(_inode);
                inode_id_list.insert(inode_id_list.end(), sub_inode_id_list.begin(), sub_inode_id_list.end());
            }
            return inode_id_list;
        }
    }

    void remove_file(string &path, bool recursive = false)
    {

        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        // 寻找 path 的 inode
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);

        // 如果 file_inode_id 为 -1，则说明文件不存在
        if (file_inode_id == -1)
        {
            cout << "文件 " << absolute_path << " 不存在" << endl;
            return;
        }

        // 获取目录和文件的 inode
        INode dir_inode = inode_table[dir_inode_id];
        INode file_inode = inode_table[file_inode_id];

        // 如果是目录，recursive 为 false，则不允许删除
        if (file_inode.file_type == 'd' && !recursive)
        {
            cout << "文件 " << absolute_path << " 是目录，不允许删除" << endl;
            return;
        }

        // 如果是文件
        if (file_inode.file_type == 'f')
        {
            vector<string> path_vector = _get_path_vector(path);
            string filename = path_vector.back();

            // 删除所有数据块
            vector<short> block_id_list = _get_block_list(file_inode);
            for (auto id : block_id_list)
            {
                _delete_block(id);
            }

            // 删除所有间接地址块
            vector<short> *indirect_block_list = _get_indirect_addr_block_list(file_inode);
            for (auto id : *indirect_block_list)
            {
                _delete_block(id);
            }

            // 删除目录项
            delete_dentry(dir_inode, filename);
            cout << "文件 " << path << " 删除成功" << endl;

            // 删除文件 inode
            _delete_inode(file_inode_id);
        }

        // // 如果是目录，并且 recursive 为 true
        // if (file_inode.file_type == 'd' && recursive)
        // {
        //     // 获取目录下所有 inode_id
        //     vector<short> inode_id_list = get_all_sub_inode_id(file_inode);
        //     // 逐个删除
        //     for (auto id : inode_id_list)
        //     {
        //         INode inode = inode_table[id];
        //         if (inode.file_type == 'f')
        //         {
        //             // 删除所有数据块
        //             vector<short> block_id_list = _get_block_list(inode);
        //             for (auto id : block_id_list)
        //             {
        //                 _delete_block(id);
        //             }

        //             // 删除所有间接地址块
        //             vector<short> *indirect_block_list = _get_indirect_addr_block_list(inode);
        //             for (auto id : *indirect_block_list)
        //             {
        //                 _delete_block(id);
        //             }

        //             // 删除目录项
        //             delete_dentry(dir_inode, filename);
        //             printf("文件 %s 删除成功\n", path);

        //             // 删除文件 inode
        //             _delete_inode(id);
        //         }
        //     }
        //     for (auto id : inode_id_list)
        //     {
        //         INode inode = inode_table[id];
        //         if (inode.file_type == 'd')
        //         {
        //             // 删除目录项
        //             delete_dentry(dir_inode, filename);
        //             printf("目录 %s 删除成功\n", path);

        //             // 删除目录 inode
        //             _delete_inode(id);
        //         }
        //     }
        // }
    }

    // 传入文件路径和文件大小（KB），创建文件
    void create_file(const string path, int filesize)
    {
        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        // 寻找 path 的 inode
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);

        // 如果 file_inode_id 不为 -1，则说明文件已存在
        if (file_inode_id != -1)
        {
            cout << "文件 " << absolute_path << " 已存在" << endl;
            return;
        }

        // 获取目录 inode
        INode dir_inode = inode_table[dir_inode_id];
        cout << "目录 inode: " << endl;
        cout << dir_inode << endl;

        // 获取可用 inode id
        short new_inode_id = _get_avail_inode();
        cout << "new_inode_id: " << new_inode_id << endl;

        if (new_inode_id == -1)
        {
            cout << "inode 用尽" << endl;
            return;
        }

        // 根据 filesize（KB）获取可用 block id
        // 检查 superblock 中的 available_block_num 是否足够
        if (superblock->available_block_num < filesize)
        {
            cout << "block 用尽" << endl;
            return;
        }

        // 获取 block id 向量
        vector<short> block_id_list;
        for (int i = 0; i < filesize; i++)
        {
            short id = _get_avail_block();
            block_id_list.push_back(id);
        }

        cout << "block_id_list.size(): " << block_id_list.size() << endl;
        for (int i = 0; i < block_id_list.size(); i++)
            cout << "block_id_list[" << i << "]: " << block_id_list[i] << endl;

        // 创建新的 inode
        INode new_inode = INode();
        new_inode.id = new_inode_id;
        new_inode.file_type = 'f';
        new_inode.file_size = filesize * 1024;
        new_inode.create_time = Util::get_current_time();
        new_inode.modify_time = Util::get_current_time();
        new_inode.link_cnt = 1;
        cout << "new_inode: " << endl;
        cout << new_inode << endl;

        // 通过 block_id_list 向量设置 new_inode 的地址
        _set_block_list(new_inode, block_id_list);
        cout << "new_inode after _set_block_list: " << endl;
        cout << new_inode << endl;

        // 向块 id 向量中写满随机字符串
        for (int i = 0; i < block_id_list.size(); i++)
        {
            char *block = new char[BLOCK_SIZE];
            for (int j = 0; j < BLOCK_SIZE; j++)
                block[j] = rand() % 26 + 'a';
            _dump(block, BLOCK_START + block_id_list[i] * BLOCK_SIZE, BLOCK_SIZE);
            delete[] block;
        }

        // 添加目录项
        string filename = _get_filename(path);
        _add_dentry(dir_inode, new_inode_id, filename);

        // 保存 inode
        inode_table[new_inode_id] = new_inode;
        _dump(&new_inode, INODE_TABLE_START + new_inode_id * INODE_SIZE, INODE_SIZE);

        // 保存 bitmap
        _save_bitmap();

        cout << "文件 " << absolute_path << " 创建成功" << endl;
    }

    char *_load_file(INode inode)
    {
        cout << "读取文件内容中 ..." << endl;
        cout << inode << endl;
        vector<short> block_id_list = _get_block_list(inode);
        for (int i = 0; i < block_id_list.size(); i++)
            cout << "block_id_list[" << i << "]: " << block_id_list[i] << endl;
        char *file_data = new char[inode.file_size];
        for (int i = 0; i < block_id_list.size(); i++)
            _load(file_data + i * BLOCK_SIZE, BLOCK_START + block_id_list[i] * BLOCK_SIZE, BLOCK_SIZE);
        return file_data;
    }

    int delete_dentry(INode inode, string &filename)
    {
        cout << "正在删除目录项 " << filename << " ..." << endl;

        // 如果不是目录
        if (inode.file_type != 'd')
        {
            printf("id 为 %d 的 inode 不是目录\n", inode.id);
            return -1;
        }

        // 获取当前目录的数据块
        vector<short> block_id_list = _get_block_list(inode);
        // 遍历数据块，找到对应的 Dentry 并删除
        for (int i = 0; i < block_id_list.size(); i++)
        {
            Dentry *dentry = new Dentry[DENTRY_NUM_PER_BLOCK];
            fseek(fp, BLOCK_START + block_id_list[i] * BLOCK_SIZE, SEEK_SET);
            fread(dentry, BLOCK_SIZE, 1, fp);
            for (int j = 0; j < DENTRY_NUM_PER_BLOCK; j++)
            {
                if (strcmp(dentry[j].filename, filename.c_str()) == 0)
                {
                    dentry[j].inode_id = -1;
                    memset(dentry[j].filename, 0, MAX_FILENAME_SIZE);
                    fseek(fp, BLOCK_START + block_id_list[i] * BLOCK_SIZE, SEEK_SET);
                    fwrite(dentry, BLOCK_SIZE, 1, fp);
                    delete[] dentry;
                    return 0;
                }
            }
            delete[] dentry;
        }
    };

    // 读取并打印文件内容
    void cat(const string &path)
    {
        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        // 寻找 path 的 inode
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);

        // 如果 file_inode_id 为 -1，则说明文件不存在
        if (file_inode_id == -1)
        {
            cout << "文件 " << absolute_path << " 不存在" << endl;
            return;
        }

        // 获取文件 inode
        INode file_inode = inode_table[file_inode_id];

        // 如果不是文件
        if (file_inode.file_type != 'f')
        {
            cout << "文件 " << absolute_path << " 不是文件" << endl;
            return;
        }

        // 读取文件内容
        char *file_data = _load_file(file_inode);
        cout << "文件 " << absolute_path << " 内容：" << endl;
        cout << file_data << endl;
    }

    // 传入目录路径，创建目录
    void create_dir(string &path)
    {
        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        // 寻找 path 的 inode
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);

        // 如果 file_inode_id 不为 -1，则说明文件已存在
        if (file_inode_id != -1)
        {
            cout << "文件 " << absolute_path << " 已存在" << endl;
            return;
        }

        // 获取目录 inode
        INode dir_inode = inode_table[dir_inode_id];

        // 获取可用 inode id
        short new_inode_id = _get_avail_inode();
        if (new_inode_id == -1)
        {
            cout << "inode 用尽" << endl;
            return;
        }

        // 创建新的 inode
        INode new_inode = INode();
        new_inode.id = new_inode_id;
        new_inode.file_type = 'd';
        new_inode.file_size = BLOCK_SIZE;
        new_inode.create_time = Util::get_current_time();
        new_inode.modify_time = Util::get_current_time();
        new_inode.link_cnt = 0;
        new_inode.direct_block[0] = _get_avail_block();

        // 创建新的 Dentry
        Dentry new_dentry;
        new_dentry.inode_id = new_inode_id;
        const char *filename = path.c_str();
        strncpy(new_dentry.filename, filename, MAX_FILENAME_SIZE - 1);

        // 将 Dentry 写入 block
        // 遍历 block，找到第一个空闲的 Dentry 位置
        vector<short> block_id_list = _get_block_list(new_inode);
        for (int i = 0; i < block_id_list.size(); i++)
        {
            Dentry *dentry = new Dentry[DENTRY_NUM_PER_BLOCK];
            fseek(fp, BLOCK_START + block_id_list[i] * BLOCK_SIZE, SEEK_SET);
            fread(dentry, BLOCK_SIZE, 1, fp);
            for (int j = 0; j < DENTRY_NUM_PER_BLOCK; j++)
            {
                if (dentry[j].inode_id == -1)
                {
                    dentry[j] = new_dentry;
                    fseek(fp, BLOCK_START + block_id_list[i] * BLOCK_SIZE, SEEK_SET);
                    fwrite(dentry, BLOCK_SIZE, 1, fp);
                    delete[] dentry;
                    break;
                }
            }
            delete[] dentry;
        }

        // 保存 inode
        inode_table[new_inode_id] = new_inode;
        _dump(&new_inode, INODE_TABLE_START + new_inode_id * INODE_SIZE, INODE_SIZE);

        // 保存 bitmap
        _save_bitmap();

        cout << "目录 " << absolute_path << " 创建成功" << endl;
    }

    // 改变当前工作目录
    int change_dir(string path)
    {
        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        // 寻找 path 的 inode
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);

        // 如果 dir_inode_id 为 -1，则说明目录不存在
        if (dir_inode_id == -1)
        {
            cout << "目录 " << absolute_path << " 不存在" << endl;
            return -1;
        }

        else if (file_inode_id != -1)
        {
            // 检查是否是目录
            if (inode_table[file_inode_id].file_type != 'd')
            {
                cout << absolute_path << " 不是目录" << endl;
                return -1;
            }
            else if (inode_table[file_inode_id].file_type == 'd')
            {
                // 更新当前工作目录
                working_dir = absolute_path;
                working_dir_inode_id = dir_inode_id;

                cout << "当前工作目录：" << working_dir << endl;
                return 0;
            }
        }
    }

    // 列出当前工作目录下的所有文件和目录
    void dir()
    {
        cout << "列出当前工作目录下的所有文件和目录 ..." << endl;
        cout << "当前工作目录：" << working_dir << endl;
        cout << "当前工作目录 inode id：" << working_dir_inode_id << endl;

        // 获取当前目录的数据块
        vector<Dentry> dentry_list = _load_dentries(working_dir_inode_id);

        // 打印当前目录下的所有文件和目录
        cout << "文件名\tinode_id" << endl;
        for (auto dentry : dentry_list)
            cout << dentry.filename << '\t' << dentry.inode_id << endl;
    }

    int cp(string &src, string &dst)
    {
        // 将路径转为绝对路径
        string absolute_src = _absolute_path(src);
        string absolute_dst = _absolute_path(dst);

        // 寻找 src 的 inode
        short src_dir_inode_id, src_file_inode_id;
        _search_inode(src, src_dir_inode_id, src_file_inode_id);

        // 寻找 dst 的 inode
        short dst_dir_inode_id, dst_file_inode_id;
        _search_inode(dst, dst_dir_inode_id, dst_file_inode_id);

        // 如果 src_file_inode_id 为 -1，则说明文件不存在
        if (src_file_inode_id == -1)
        {
            cout << "文件 " << absolute_src << " 不存在" << endl;
            return -1;
        }

        // 如果 dst_file_inode_id 不为 -1，则说明文件已存在
        if (dst_file_inode_id != -1)
        {
            cout << "文件 " << absolute_dst << " 已存在" << endl;
            return -1;
        }

        // 获取 src 文件 inode
        INode src_file_inode = inode_table[src_file_inode_id];

        // 获取 dst 目录 inode
        INode dst_dir_inode = inode_table[dst_dir_inode_id];

        // 如果 src 是文件
        if (src_file_inode.file_type == 'f')
        {
            // 获取可用 inode id
            short new_inode_id = _get_avail_inode();
            if (new_inode_id == -1)
            {
                printf("inode 用尽\n");
                return -1;
            }

            // 根据 src 文件大小获取可用 block id
            // 检查 superblock 中的 available_block_num 是否足够
            if (superblock->available_block_num < src_file_inode.file_size / BLOCK_SIZE)
            {
                printf("block 用尽\n");
                return -1;
            }

            // 获取 block id 向量
            vector<short> block_id_list;
            for (int i = 0; i < src_file_inode.file_size / BLOCK_SIZE; i++)
            {
                short id = _get_avail_block();
                block_id_list.push_back(id);
            }

            // 创建新的 inode
            INode new_inode = INode();
            new_inode.id = new_inode_id;
            new_inode.file_type = 'f';
            new_inode.file_size = src_file_inode.file_size;
            new_inode.create_time = Util::get_current_time();
            new_inode.modify_time = Util::get_current_time();
            new_inode.link_cnt = 1;
            // 通过 block_id_list 向量设置 new_inode 的地址
            _set_block_list(new_inode, block_id_list);

            // 向块 id 向量中写入 src 文件内容
            char *file_data = _load_file(src_file_inode);
            for (int i = 0; i < block_id_list.size(); i++)
            {
                // 直接从 file_data 写到 block_id_list 对应的 block 中
                fseek(fp, BLOCK_START + block_id_list[i] * BLOCK_SIZE, SEEK_SET);
                fwrite(file_data + i * BLOCK_SIZE, BLOCK_SIZE, 1, fp);
            }

            // 添加目录项
            // 获取 dst 文件名
            vector<string> dst_vector = _get_path_vector(dst);
            string dst = dst_vector.back();
            // char *filename = new char[dst.length() + 1];
            // strcpy(filename, dst.c_str());
            _add_dentry(dst_dir_inode, new_inode_id, dst);

            // 保存 inode
            inode_table[new_inode_id] = new_inode;
            _dump(&new_inode, INODE_TABLE_START + new_inode_id * INODE_SIZE, INODE_SIZE);

            // 保存 bitmap
            _save_bitmap();

            cout << "文件 " << absolute_src << " 复制到 " << absolute_dst << " 成功" << endl;

            return 0;
        }

        // // 如果 src 是目录
        // // （待审查）

        // // 获取目录下所有 inode_id
        // vector<short> inode_id_list = get_all_sub_inode_id(src_file_inode);

        // // 逐个复制
        // for (auto id : inode_id_list)
        // {
        //     INode inode = inode_table[id];
        //     if (inode.file_type == 'f')
        //     {
        //         // 获取可用 inode id
        //         short new_inode_id = _get_avail_inode();
        //         if (new_inode_id == -1)
        //         {
        //             printf("inode 用尽\n");
        //             return -1;
        //         }

        //         // 根据 src 文件大小获取可用 block id
        //         // 检查 superblock 中的 available_block_num 是否足够
        //         if (superblock->available_block_num < inode.file_size / BLOCK_SIZE)
        //         {
        //             printf("block 用尽\n");
        //             return -1;
        //         }

        //         // 获取 block id 向量
        //         vector<short> block_id_list;
        //         for (int i = 0; i < inode.file_size / BLOCK_SIZE; i++)
        //         {
        //             short id = _get_avail_block();
        //             block_id_list.push_back(id);
        //         }

        //         // 创建新的 inode
        //         INode new_inode = INode();
        //         new_inode.id = new_inode_id;
        //         new_inode.file_type = 'f';
        //         new_inode.file_size = inode.file_size;
        //         new_inode.create_time = Util::get_current_time();
        //         new_inode.modify_time = Util::get_current_time();
        //         new_inode.link_cnt = 1;
        //         // 通过 block_id_list 向量设置 new_inode 的地址
        //         _set_block_list(new_inode, block_id_list);

        //         // 向块 id 向量中写入 src 文件内容
        //         char *file_data = _load_file(inode);
        //         for (int i = 0; i < block_id_list.size(); i++)
        //         {
        //             char *block = new char[BLOCK_SIZE];
        //             for (int j = 0; j < BLOCK_SIZE; j++)
        //             {
        //                 block[j] = file_data[i * BLOCK_SIZE + j];
        //             }
        //             fseek(fp, BLOCK_START + block_id_list[i] * BLOCK_SIZE, SEEK_SET);
        //             fwrite(block, BLOCK_SIZE, 1, fp);
        //             delete[] block;
        //         }

        //         // 添加目录项
        //         _add_dentry(dst_dir_inode, dst, new_inode_id);

        //         // 保存 inode
        //         inode_table[new_inode_id] = new_inode;
        //         save_inode(new_inode_id);

        //         // 保存 bitmap
        //         save_bitmap();
        //     }
        // }
        // for (auto id : inode_id_list)
        // {
        //     INode inode = inode_table[id];
        //     if (inode.file_type == 'd')
        //     {
        //         // 获取可用 inode id
        //         short new_inode_id = _get_avail_inode();
        //         if (new_inode_id == -1)
        //         {
        //             printf("inode 用尽\n");
        //             return -1;
        //         }

        //         // 创建新的 inode
        //         INode new_inode = INode();
        //         new_inode.id = new_inode_id;
        //         new_inode.file_type = 'd';
        //         new_inode.file_size = BLOCK_SIZE;
        //         new_inode.create_time = Util::get_current_time();
        //         new_inode.modify_time = Util::get_current_time();
        //         new_inode.link_cnt = 0;
        //         new_inode.direct_block[0] = _get_avail_block();

        //         // 创建新的 Dentry
        //         Dentry new_dentry;
        //         new_dentry.inode_id = new_inode_id;
        //         strcpy(new_dentry.filename, path);

        //         // 将 Dentry 写入 block
        //         // 遍历 block，找到第一个空闲的 Dentry 位置
        //         vector<short> block_id_list = new_inode._get_block_list();
        //         for (int i = 0; i < block_id_list.size(); i++)
        //         {
        //             Dentry *dentry = new Dentry[DENTRY_NUM_PER_BLOCK];
        //             fseek(fp, BLOCK_START + block_id_list[i] * BLOCK_SIZE, SEEK_SET);
        //             fread(dentry, BLOCK_SIZE, 1, fp);
        //             for (int j = 0; j < DENTRY_NUM_PER_BLOCK; j++)
        //             {
        //                 if (dentry[j].inode_id == -1)
        //                 {
        //                     dentry[j] = new_dentry;
        //                     fseek(fp, BLOCK_START + block_id_list[i] * BLOCK_SIZE, SEEK_SET);
        //                     fwrite(dentry, BLOCK_SIZE, 1, fp);
        //                     delete[] dentry;
        //                     break;
        //                 }
        //             }
        //             delete[] dentry;
        //         }

        //         // 添加目录项
        //         dst_dir_inode._add_dentry(dst, new_inode_id);

        //         // 保存 inode
        //         inode_table[new_inode_id] = new_inode;
        //         save_inode(new_inode_id);

        //         // 保存 bitmap
        //         save_bitmap();
        //     }
        // }

        // printf("目录 %s 复制到 %s 成功\n", absolute_src, absolute_dst);
        return 0;
    }

    // void sum()
    // {
    //     printf("文件系统总大小：%d KB\n", superblock->filesystem_size / 1024);
    //     printf("已用大小：%d KB\n", superblock->used_block_num * BLOCK_SIZE / 1024);
    //     printf("剩余大小：%d KB\n", superblock->available_block_num * BLOCK_SIZE / 1024);
    // }
};

void _read_write_filesys_example_()
{
    FileSystem fs;

    // 读入单个
    Dentry *dentry = new Dentry;
    fs._load(dentry, BLOCK_START, 32);
    cout << dentry->inode_id << endl;
    cout << dentry->filename << endl;

    // 读入数组
    Dentry *dentry_list = new Dentry[DENTRY_NUM_PER_BLOCK];
    fs._load(dentry_list, BLOCK_START, BLOCK_SIZE);
    for (int i = 0; i < DENTRY_NUM_PER_BLOCK; i++)
    {
        cout << "dentry[" << i << "].inode_id: " << dentry_list[i].inode_id << endl;
        cout << "dentry[" << i << "].filename: " << dentry_list[i].filename << endl;
    }
}

int main(int argc, char *argv[])
{

    info << "hello world" << endl;
    exit(0);

    print_all_macros();
    // // 打印 INode 大小
    // printf("INode size: %d\n", (int)sizeof(INode)); // 48
    // // 打印 File 大小
    // printf("Dentry size: %d\n", (int)sizeof(Dentry)); // 32
    // // 打印 DATA_BLOCK_NUM
    // printf("数据块数量: %d\n", DATA_BLOCK_NUM);
    FileSystem fs;

    // // 打印 block_bitmap 的大小
    // printf("Block bitmap size: %d\n", BLOCK_BITMAP_SIZE); // 2048
    // // 打印 inode_bitmap 的大小
    // printf("INode bitmap size: %d\n", INODE_BITMAP_SIZE); // 1024
    // // 打印时间测试
    // cout << fs.time_to_string(fs.get_current_time()) << endl;

    // string a = "革命尚未成功，同志仍需努力";
    // cout << "sizeof(a): " << sizeof(a) << "  strlen(a): " << strlen(a.c_str()) << "  a.length(): " << a.length() << "  a.size(): " << a.size() << endl;

    // fs.create_file("/abc", 15834);

    // 打印根目录所有文件
    fs.dir();
    // fs.cat("/abc");

    return 0;
}
