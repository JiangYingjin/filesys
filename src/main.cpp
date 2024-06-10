#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <regex>

#include <vector>
#include <map>
#include <unordered_set>

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <memory>
#include <cassert>
#include <chrono>
#include <csignal>

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Appenders/ConsoleAppender.h>

// #include <boost/algorithm/string.hpp>

using namespace std;
#define dout PLOGD

#define FILESYSTEM_NAME "file.sys"
#define FILESYSTEM_SIZE (16 * 1024 * 1024) // 16MB
#define BLOCK_SIZE (1024)
#define INODE_SIZE (64)
#define BLOCK_NUM (FILESYSTEM_SIZE / BLOCK_SIZE) // 16K
#define INODE_NUM (BLOCK_NUM / 2)                // 8K
#define MAX_FILE_SIZE ((NUM_DIRECT_BLOCK + NUM_INDIRECT_BLOCK * ADDRESS_PER_BLOCK + NUM_DOUBLE_INDIRECT_BLOCK * ADDRESS_PER_BLOCK * ADDRESS_PER_BLOCK) * BLOCK_SIZE)
// (10 + 512 + 512 * 512) KB = 262,666 KB
#define MAX_FILENAME_SIZE (29)

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

#define RESET "\e[0m"
#define BOLD "\e[1m"
#define RED "\e[31m"
#define GREEN "\e[32m"
#define YELLOW "\e[33m"
#define BLUE "\e[34m"
#define PURPLE "\e[35m"
#define CYAN "\e[36m"

// Vector 输出重载
template <typename T>
ostream &operator<<(ostream &os, const vector<T> &vec)
{
    os << "[ ";
    for (const T &element : vec)
    {
        os << element << " ";
    }
    os << "]";
    return os;
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
    static const char *time_to_string(int32_t timestamp)
    {
        time_t t = timestamp;
        struct tm *tm = localtime(&t);
        // 格式化成 2024-01-01 12:00:00 格式
        char *datetime_str = new char[20];
        strftime(datetime_str, 20, "%Y-%m-%d %H:%M:%S", tm);
        return datetime_str;
        // return asctime(tm);
    }

    // 根据文件大小计算需要使用的块数量
    static const int block_occupation(int filesize_kb)
    {
        short num_indirect_block = filesize_kb > NUM_DIRECT_BLOCK * BLOCK_SIZE / 1024 ? 1 : 0;
        short num_double_indirect_block = filesize_kb > NUM_DIRECT_BLOCK * BLOCK_SIZE / 1024 + NUM_INDIRECT_BLOCK * ADDRESS_PER_BLOCK * BLOCK_SIZE / 1024 ? 1 : 0;
        short num_2nd_indirect_block = ceil(float(filesize_kb - NUM_DIRECT_BLOCK * BLOCK_SIZE / 1024 - NUM_INDIRECT_BLOCK * ADDRESS_PER_BLOCK * BLOCK_SIZE / 1024) / (ADDRESS_PER_BLOCK * BLOCK_SIZE / 1024));
        // cout << "filesize_kb: " << filesize_kb << " num_indirect_block: " << num_indirect_block << " num_double_indirect_block: " << num_double_indirect_block << " num_2nd_indirect_block: " << num_2nd_indirect_block << endl;
        return filesize_kb + num_indirect_block + num_double_indirect_block + num_2nd_indirect_block;
    }

    static const string readable_size(const int size)
    {
        vector<string> suffix = {"", "K", "M"};
        double _new_size = size;
        int _suffix_idx = 0;

        while (_new_size > 999)
        {
            _new_size /= 1024;
            _suffix_idx++;
        }

        stringstream ss;
        if (_suffix_idx > 0)
            if (_new_size < 10)
                ss << fixed << setprecision(1);
            else
                ss << fixed << setprecision(0);
        ss << _new_size;

        return ss.str() + suffix[_suffix_idx];
    }

    static bool ends_with(const string &str, const string &suffix)
    {
        if (str.length() < suffix.length())
            return false;

        return str.substr(str.length() - suffix.length()) == suffix;
    }

    static string trim_space(const string &str)
    {
        size_t first = str.find_first_not_of(' ');
        if (string::npos == first)
            return "";
        size_t last = str.find_last_not_of(' ');
        return str.substr(first, (last - first + 1));
    }

    static vector<string> split_space(const string &str)
    {
        vector<string> tokens;
        istringstream iss(str);
        string token;
        while (iss >> token)
            tokens.push_back(token);
        return tokens;
    }

    static void cout_center(const string str, const int width)
    {
        if (width < str.size())
        {
            cout << "[居中打印] 设定的打印宽度为 " << width << "，小于字符串长度 " << str.size() << "，居中打印失败！" << endl;
            return;
        }

        int padding_left = (width - str.size()) / 2;
        int right_padding = width - str.size() - padding_left;
        cout << string(padding_left, ' ') << str << string(right_padding, ' ');
    }
};

class SuperBlock
{
public:
    const int filesystem_size = FILESYSTEM_SIZE; // 文件系统大小（Byte）
    const int block_size = BLOCK_SIZE;           // 块（block）的大小（Byte）
    const int block_num = BLOCK_NUM;             // 块（block）的数量
    const int data_block_num = DATA_BLOCK_NUM;   // 数据块的数量
    const int inode_size = INODE_SIZE;           // INode 的大小（Byte）
    const int inode_num = INODE_NUM;             // INode 的数量

    int available_block_num = (FILESYSTEM_SIZE - BLOCK_START) / BLOCK_SIZE; // 可用块的数量
    int available_inode_num = INODE_NUM;                                    // 可用 INode 的数量

    SuperBlock()
    {
    }

    friend ostream &operator<<(ostream &os, const SuperBlock &superblock)
    {
        // os << "--------------- 超级块信息 ---------------" << endl;
        // os << "文件系统大小：\t\t" << float(superblock.filesystem_size) / 1024 / 1024 << " MB" << endl;
        // os << "块（Block）大小：\t" << superblock.block_size << " Byte" << endl;
        // os << "块（Block）数量：\t" << superblock.block_num << endl;
        // os << "数据块数量：\t\t" << superblock.data_block_num << endl;
        // os << "INode 大小：\t\t" << superblock.inode_size << " Byte" << endl;
        // os << "INode 数量：\t\t" << superblock.inode_num << endl;
        // os << "------------------------------------------" << endl;
        // os << "已占用空间：\t\t" << (superblock.block_num - superblock.available_block_num) * superblock.block_size << " Byte" << endl;
        // os << "可用空间：\t\t" << superblock.available_block_num * superblock.block_size << " Byte" << endl;
        // os << "已用块数量：\t\t" << superblock.block_num - superblock.available_block_num << endl;
        // os << "可用块数量：\t\t" << superblock.available_block_num << endl;
        // os << "已用 INode 数量：\t" << superblock.inode_num - superblock.available_inode_num << endl;
        // os << "可用 INode 数量：\t" << superblock.available_inode_num << endl;

        os << "------------ SuperBlock Info -------------" << endl;
        os << "Filesystem Size:\t" << Util::readable_size(superblock.filesystem_size) << endl;
        os << "Block Size:\t\t" << superblock.block_size << " Byte" << endl;
        os << "Block Num:\t\t" << superblock.block_num << endl;
        os << "Data Block Num:\t\t" << superblock.data_block_num << endl;
        os << "INode Size:\t\t" << superblock.inode_size << " Byte" << endl;
        os << "INode Num:\t\t" << superblock.inode_num << endl;
        os << "------------------------------------------" << endl;
        os << "Used Space:\t\t" << Util::readable_size((superblock.block_num - superblock.available_block_num) * superblock.block_size) << endl;
        os << "Available Space:\t" << Util::readable_size(superblock.available_block_num * superblock.block_size) << endl;
        os << "Used Block Num:\t\t" << superblock.block_num - superblock.available_block_num << endl;
        os << "Available Block Num:\t" << superblock.available_block_num << endl;
        os << "Used INode Num:\t\t" << superblock.inode_num - superblock.available_inode_num << endl;
        os << "Available INode Num:\t" << superblock.available_inode_num << endl;
        os << "------------------------------------------" << endl;
        return os;
    }

    // 复制构造函数
    SuperBlock(const SuperBlock &superblock)
        : filesystem_size(superblock.filesystem_size), block_size(superblock.block_size), block_num(superblock.block_num), data_block_num(superblock.data_block_num), inode_size(superblock.inode_size), inode_num(superblock.inode_num), available_block_num(superblock.available_block_num), available_inode_num(superblock.available_inode_num)
    {
    }

    // 赋值运算符重载
    SuperBlock &operator=(const SuperBlock &superblock)
    {
        this->available_block_num = superblock.available_block_num;
        this->available_inode_num = superblock.available_inode_num;
        return *this;
    }
};

class Dentry
{
public:
    short inode_id;
    char filename[MAX_FILENAME_SIZE + 1]; // 最后一位必须保留为 '\0'

    Dentry()
        : inode_id(-1)
    {
        set_filename("unknown");
    }

    Dentry(short inode_id, string filename)
        : inode_id(inode_id)
    {
        set_filename(filename);
    }

    void set_filename(string filename_)
    {
        if (filename_.length() > MAX_FILENAME_SIZE)
        {
            cout << "[设置目录项文件名] 文件名过长，filename.size() 为 " << filename_.size() << "，应小于最大长度 " << MAX_FILENAME_SIZE << endl;
            filename_ = filename_.substr(0, MAX_FILENAME_SIZE);
            cout << "[设置目录项文件名] 截断文件名存入：" << filename_ << endl;
        }
        memset(filename, 0, MAX_FILENAME_SIZE + 1);
        strcpy(filename, filename_.c_str());
        // // 打印 filename 和 filename_.c_str() 的大小
        // cout << "[设置目录项文件名] filename.size(): " << filename_.size() << " filename_.c_str() size: " << strlen(filename_.c_str()) << endl;
        // cout << "[设置目录项文件名] filename: " << filename << endl;
    }

    string get_filename() const
    {
        return string(filename);
    }

    friend ostream &operator<<(ostream &os, const Dentry &dentry)
    {
        os << "--------------- 目录项信息 ---------------" << endl;
        os << "INode ID：\t" << dentry.inode_id << endl;
        os << "文件名：\t" << dentry.filename << endl;
        os << "------------------------------------------" << endl;
        return os;
    }

    // 复制构造函数
    Dentry(const Dentry &dentry)
        : inode_id(dentry.inode_id)
    {
        strcpy(this->filename, dentry.filename);
        // this->set_filename(dentry.get_filename());
    }

    // 赋值运算符重载
    Dentry &operator=(const Dentry &dentry)
    {
        this->inode_id = dentry.inode_id;
        strcpy(this->filename, dentry.filename);
        // this->set_filename(dentry.get_filename());
        return *this;
    }
};

class INode
{
public:
    short id;                                               // INode 编号，占用 2 Byte
    char file_type;                                         // 文件类型，占用 1 Byte f d
    int file_size;                                          // 文件大小（单位 Byte），占用 4 Byte
    int32_t create_time;                                    // 创建时间，占用 4 Byte
    int32_t modify_time;                                    // 修改时间，占用 4 Byte
    short link_cnt;                                         // 链接数，占用 2 Byte
    short direct_block[NUM_DIRECT_BLOCK];                   // 直接地址，占用 20 Byte
    short indirect_block[NUM_INDIRECT_BLOCK];               // 间接地址，占用 2 Byte
    short double_indirect_block[NUM_DOUBLE_INDIRECT_BLOCK]; // 双重间接地址，占用 2 Byte
                                                            // 总共 41 Byte
    INode()
        : id(-1), file_type('\0'), file_size(0), create_time(0), modify_time(0), link_cnt(0)
    {
        clear_address();
    }

    void clear_address()
    {
        fill_n(direct_block, NUM_DIRECT_BLOCK, -1);
        fill_n(indirect_block, NUM_INDIRECT_BLOCK, -1);
        fill_n(double_indirect_block, NUM_DOUBLE_INDIRECT_BLOCK, -1);
    }

    friend ostream &operator<<(ostream &os, const INode &inode)
    {
        // os << "--------------- INode 信息 ---------------" << endl;
        // os << "INode ID：\t" << inode.id << endl;
        // os << "文件类型：\t" << inode.file_type << endl;
        // os << "文件大小：\t" << inode.file_size << " 字节" << endl;
        // os << "创建时间：\t" << Util::time_to_string(inode.create_time) << endl;
        // os << "修改时间：\t" << Util::time_to_string(inode.modify_time) << endl;
        // os << "硬链接数：\t" << inode.link_cnt << endl;
        os << "------------------- INode Info -------------------" << endl;
        os << "INode ID:\t\t" << inode.id << endl;
        os << "File Type:\t\t" << inode.file_type << endl;
        os << "File Size:\t\t" << Util::readable_size(inode.file_size) << endl;
        os << "Create Time:\t\t" << Util::time_to_string(inode.create_time) << endl;
        os << "Modify Time:\t\t" << Util::time_to_string(inode.modify_time) << endl;
        os << "Link Count:\t\t" << inode.link_cnt << endl;

        // os << "直接块：\t";
        os << "Direct Addr:\t\t";
        for (int i = 0; i < NUM_DIRECT_BLOCK; i++)
            os << inode.direct_block[i] << " ";
        os << endl;

        // os << "间接块：\t";
        os << "Indirect Addr:\t\t";
        for (int i = 0; i < NUM_INDIRECT_BLOCK; i++)
            os << inode.indirect_block[i] << " ";
        os << endl;

        // os << "二级间接块：\t";
        os << "Double Indirect Addr:\t";
        for (int i = 0; i < NUM_DOUBLE_INDIRECT_BLOCK; i++)
            os << inode.double_indirect_block[i] << " ";
        os << endl;

        os << "--------------------------------------------------" << endl;
        return os;
    }
};

class Bitmap
{
public:
    vector<char> bitmap;
    // char *bitmap;

    Bitmap(int size_byte)
    {
        bitmap.resize(size_byte, 0);
        // bitmap = new char[size_byte];
        // memset(bitmap, 0, size_byte);
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

    // 复制构造函数
    Bitmap(const Bitmap &bitmap)
    {
        this->bitmap = bitmap.bitmap;
    }

    // 赋值运算符重载
    Bitmap &operator=(const Bitmap &bitmap)
    {
        this->bitmap = bitmap.bitmap;
        return *this;
    }
};

class FileSystem
{

public:
    // 持久化数据
    SuperBlock superblock;
    Bitmap block_bitmap = Bitmap(BLOCK_BITMAP_SIZE);
    Bitmap inode_bitmap = Bitmap(INODE_BITMAP_SIZE);

    // 运行时数据
    string working_dir;
    short working_dir_inode_id;

    const int SUPERBLOCK_CLASS_SIZE;
    const int INODE_CLASS_SIZE;

    FileSystem()
        : SUPERBLOCK_CLASS_SIZE(sizeof(SuperBlock)), INODE_CLASS_SIZE(sizeof(INode))
    {
        if (!_is_filesys_exist())
        {
            // cout << "[文件系统初始化] 文件系统不存在，创建中 ..." << endl;
            cout << "[Init] File system does not exist, creating ..." << endl;
            _create_filesys();
            // cout << "[文件系统初始化] 文件系统创建成功！" << endl;
            cout << "[Init] File system created successfully!" << endl;
        }
        else
        {
            // cout << "[文件系统初始化] 文件系统已存在，加载中 ..." << endl;
            cout << "[Init] File system already exists, loading ..." << endl;
            _load(&superblock, SUPERBLOCK_START, SUPERBLOCK_CLASS_SIZE);
            _load(block_bitmap.bitmap.data(), BLOCK_BITMAP_START, BLOCK_BITMAP_SIZE);
            _load(inode_bitmap.bitmap.data(), INODE_BITMAP_START, INODE_BITMAP_SIZE);
            // cout << "[文件系统初始化] 文件系统加载成功！" << endl;
            cout << "[Init] File system loaded successfully!" << endl;
        }
        _init_working_dir();
    }

    // 赋值构造函数
    FileSystem(const FileSystem &fs)
        : superblock(fs.superblock), block_bitmap(fs.block_bitmap), inode_bitmap(fs.inode_bitmap), working_dir(fs.working_dir), working_dir_inode_id(fs.working_dir_inode_id), SUPERBLOCK_CLASS_SIZE(fs.SUPERBLOCK_CLASS_SIZE), INODE_CLASS_SIZE(fs.INODE_CLASS_SIZE)
    {
    }

    // 重载赋值运算符
    FileSystem &operator=(const FileSystem &fs)
    {
        superblock = fs.superblock;
        block_bitmap = fs.block_bitmap;
        inode_bitmap = fs.inode_bitmap;
        working_dir = fs.working_dir;
        working_dir_inode_id = fs.working_dir_inode_id;
        return *this;
    }

    ~FileSystem()
    {
        _dump_header();
        // cout << "[文件系统退出] 文件系统元数据已保存，退出成功！" << endl;
        cout << "[Exit] File system metadata saved, exit successfully!" << endl;
    }

    void _dump(const void *data, int pos, int size)
    {
        fstream file(FILESYSTEM_NAME, ios::in | ios::out | ios::binary);
        if (!file)
            // cout << "文件打开失败" << endl;
            cout << "File open failed" << endl;
        file.seekp(pos, ios::beg);
        file.write((char *)data, size);
        file.close();
    }

    // 将文件数据加载到内存（请特别小心 size 的设置，以免导致堆栈粉碎）
    void _load(void *data, int pos, int size)
    {
        fstream file(FILESYSTEM_NAME, ios::in | ios::out | ios::binary);
        if (!file)
            // cout << "文件打开失败" << endl;
            cout << "File open failed" << endl;
        file.seekg(pos, ios::beg);
        file.read((char *)data, size);
        file.close();
    }

    // 创建文件系统
    void _create_filesys()
    {
        fstream file(FILESYSTEM_NAME, ios::out | ios::binary | ios::trunc);
        char *data = new char[FILESYSTEM_SIZE];
        memset(data, 0, FILESYSTEM_SIZE);
        file.write(data, FILESYSTEM_SIZE);
        file.close();
        delete[] data;

        _dump_header();

        _init_root_dir();
    }

    // 初始化根目录
    void _init_root_dir()
    {
        dout << "[初始化根目录] 准备初始化 ..." << endl;

        // Inode
        _save_inode(_get_avail_inode(), 'd', BLOCK_SIZE);
        // 数据块
        _create_blank_dentries(_get_avail_block(), ROOT_INODE_ID, ROOT_INODE_ID);

        dout << "[初始化根目录] 初始化完成！" << endl;
    }

    void _init_working_dir()
    {
        working_dir = "/";
        working_dir_inode_id = ROOT_INODE_ID;
        dout << "[初始化工作目录] 当前工作目录为 " << working_dir << "（INode ID 为 " << working_dir_inode_id << "）" << endl;
    }

    bool _is_filesys_exist()
    {
        ifstream file(FILESYSTEM_NAME);
        bool exist = file.good();
        file.close();
        return exist;
    }

    void _load_header()
    {
        _load(&superblock, SUPERBLOCK_START, SUPERBLOCK_CLASS_SIZE);
        _load(block_bitmap.bitmap.data(), BLOCK_BITMAP_START, BLOCK_BITMAP_SIZE);
        _load(inode_bitmap.bitmap.data(), INODE_BITMAP_START, INODE_BITMAP_SIZE);
    }

    void _dump_header()
    {
        _dump(&superblock, SUPERBLOCK_START, SUPERBLOCK_CLASS_SIZE);
        _dump(block_bitmap.bitmap.data(), BLOCK_BITMAP_START, BLOCK_BITMAP_SIZE);
        _dump(inode_bitmap.bitmap.data(), INODE_BITMAP_START, INODE_BITMAP_SIZE);
    }

    // 获取可用块 ID 并在 bitmap 中标记已使用
    short _get_avail_block()
    {
        for (int i = 0; i < superblock.block_num; i++)
            if (!block_bitmap.get(i))
            {
                dout << "[可用块申请] 新申请：" << i << endl;
                // Bitmap
                block_bitmap.set(i);
                _dump(block_bitmap.bitmap.data(), BLOCK_BITMAP_START, BLOCK_BITMAP_SIZE);
                // Superblock
                superblock.available_block_num--;
                _dump(&superblock, SUPERBLOCK_START, SUPERBLOCK_CLASS_SIZE);
                return i;
            }

        dout << "[可用块申请] 无可用块" << endl;
        return -1;
    }

    // 获取可用 inode ID 并在 bitmap 中标记已使用
    short _get_avail_inode()
    {
        for (int i = 0; i < superblock.inode_num; i++)
            if (!inode_bitmap.get(i))
            {
                dout << "[可用 INode 申请] 新申请：" << i << endl;
                // Bitmap
                inode_bitmap.set(i);
                _dump(inode_bitmap.bitmap.data(), INODE_BITMAP_START, INODE_BITMAP_SIZE);
                // Superblock
                superblock.available_inode_num--;
                _dump(&superblock, SUPERBLOCK_START, SUPERBLOCK_CLASS_SIZE);
                return i;
            }

        dout << "[可用 INode 申请] 无可用 INode" << endl;
        return -1;
    }

    void _clear_block(short id)
    {
        // Bitmap
        block_bitmap.set(id, 0);
        _dump(block_bitmap.bitmap.data(), BLOCK_BITMAP_START, BLOCK_BITMAP_SIZE);
        // Superblock
        superblock.available_block_num++;
        _dump(&superblock, SUPERBLOCK_START, SUPERBLOCK_CLASS_SIZE);
    }

    void _clear_block(vector<short> &block_id_list)
    {
        // Bitmap
        for (const auto &block_id : block_id_list)
            block_bitmap.set(block_id, 0);
        _dump(block_bitmap.bitmap.data(), BLOCK_BITMAP_START, BLOCK_BITMAP_SIZE);
        // Superblock
        superblock.available_block_num += block_id_list.size();
        _dump(&superblock, SUPERBLOCK_START, SUPERBLOCK_CLASS_SIZE);
    }

    void _clear_inode(short id)
    {
        // Bitmap
        inode_bitmap.set(id, 0);
        _dump(inode_bitmap.bitmap.data(), INODE_BITMAP_START, INODE_BITMAP_SIZE);
        // Superblock
        superblock.available_inode_num++;
        _dump(&superblock, SUPERBLOCK_START, SUPERBLOCK_CLASS_SIZE);
    }

    // 目录 d 的数据块
    void _create_blank_dentries(const short &block_id, const short &curr_dir_inode_id, const short &parent_dir_inode_id)
    {
        // dout << "[创建空 Dentry 数据块] Block ID: " << block_id << "，当前目录 INode ID: " << curr_dir_inode_id << "，父目录 INode ID: " << parent_dir_inode_id << endl;

        vector<Dentry> dentry(DENTRY_NUM_PER_BLOCK);

        dentry[0] = Dentry(curr_dir_inode_id, ".");
        INode curr_dir_inode = _get_inode(curr_dir_inode_id);
        curr_dir_inode.direct_block[0] = block_id;
        curr_dir_inode.link_cnt++;
        _save_inode(curr_dir_inode);
        // dout << "[创建空 Dentry 数据块] Curr Dir INode ID：" << curr_dir_inode_id << endl
        //      << curr_dir_inode << endl;
        // dout << "[创建空 Dentry 数据块] INode 0：" << endl
        //      << _get_inode(0) << endl;

        dentry[1] = Dentry(parent_dir_inode_id, "..");
        INode parent_dir_inode = _get_inode(parent_dir_inode_id);
        parent_dir_inode.link_cnt++;
        _save_inode(parent_dir_inode);
        // dout << "[创建空 Dentry 数据块] Parent Dir INode ID：" << parent_dir_inode_id << endl
        //      << parent_dir_inode << endl;
        // dout << "[创建空 Dentry 数据块] INode 0：" << endl
        //      << _get_inode(0) << endl;

        _dump(dentry.data(), BLOCK_START + block_id * BLOCK_SIZE, BLOCK_SIZE);
    }

    void _create_blank_dentries(const short &block_id)
    {
        vector<Dentry> dentry(DENTRY_NUM_PER_BLOCK);
        _dump(dentry.data(), BLOCK_START + block_id * BLOCK_SIZE, BLOCK_SIZE);
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
    void _set_block_list(INode &inode, const vector<short> &block_id_list)
    {
        // 清空 INode 地址
        inode.clear_address();

        // 将 block_id_list 分成直接块、间接块、双重间接块三个部分
        vector<short> _direct_block_list;
        vector<short> _indirect_block_list;
        vector<short> _double_indirect_block_list;

        dout << "[写入 INode 块地址] 共有 " << block_id_list.size() << " 个数据块" << endl;
        for (int i = 0; i < block_id_list.size(); i++)
        {
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
                dout << "[写入 INode 块地址] 直接块 " << i << "：" << inode.direct_block[i] << endl;
            }

        // 间接块
        if (!_indirect_block_list.empty())
        {
            inode.indirect_block[0] = _get_avail_block();

            short *block = new short[ADDRESS_PER_BLOCK];
            fill_n(block, ADDRESS_PER_BLOCK, -1);

            for (int j = 0; j < _indirect_block_list.size(); j++)
            {
                dout << "[写入 INode 块地址] 间接块 " << j << "：" << _indirect_block_list[j] << endl;
                block[j] = _indirect_block_list[j];
            }

            _dump(block, BLOCK_START + inode.indirect_block[0] * BLOCK_SIZE, BLOCK_SIZE);
            delete[] block;
        }

        // 二级间接块
        if (!_double_indirect_block_list.empty())
        {
            dout << "[写入 INode 块地址] 二级地址需管理共 " << _double_indirect_block_list.size() << " 个数据块" << endl;
            dout << "[写入 INode 块地址] 二级地址第一级共 " << ceil(float(_double_indirect_block_list.size()) / ADDRESS_PER_BLOCK) << " 个地址" << endl;

            inode.double_indirect_block[0] = _get_avail_block();
            short _1st_address_block[ADDRESS_PER_BLOCK]; // 二级地址块的地址
            fill_n(_1st_address_block, ADDRESS_PER_BLOCK, -1);

            for (int i = 0; i < ceil(float(_double_indirect_block_list.size()) / ADDRESS_PER_BLOCK); i++)
            {
                _1st_address_block[i] = _get_avail_block();
                dout << "[写入 INode 块地址] 二级间接块 " << i << "：" << _1st_address_block[i] << endl;

                short *block = new short[ADDRESS_PER_BLOCK];
                fill_n(block, ADDRESS_PER_BLOCK, -1);
                for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                {
                    if (i * ADDRESS_PER_BLOCK + j >= _double_indirect_block_list.size())
                        break;
                    block[j] = _double_indirect_block_list[i * ADDRESS_PER_BLOCK + j];
                    dout << "[写入 INode 块地址] 二级间接块（第二级） " << i << "，地址 " << j << "：" << block[j] << endl;
                }
                _dump(block, BLOCK_START + _1st_address_block[i] * BLOCK_SIZE, BLOCK_SIZE);
                delete[] block;
            }
            _dump(_1st_address_block, BLOCK_START + inode.double_indirect_block[0] * BLOCK_SIZE, BLOCK_SIZE);
        }

        _save_inode(inode);
    }

    void _set_block_list(const short &inode_id, const vector<short> &block_id_list)
    {
        INode inode = _get_inode(inode_id);
        assert(inode.id == inode_id);
        _set_block_list(inode, block_id_list);
    }

    // 由 inode 的直接块ID、间接块ID、双重间接块ID获取其块ID向量
    // 返回块 ID 向量，根据这个向量就能获取所有内容
    vector<short> _get_block_list(const INode &inode)
    {
        vector<short> block_id_list;

        // 直接块
        for (int i = 0; i < NUM_DIRECT_BLOCK; i++)
            if (inode.direct_block[i] != -1)
            {
                // dout << "[读取 INode 块列表] 直接块 " << i << " ：" << inode.direct_block[i] << endl;
                block_id_list.push_back(inode.direct_block[i]);
            }

        // 间接块
        for (int i = 0; i < NUM_INDIRECT_BLOCK; i++)
            if (inode.indirect_block[i] != -1)
            {
                // dout << "[读取 INode 块列表] 间接块 " << i << " ：" << inode.indirect_block[i] << endl;
                vector<short> block(ADDRESS_PER_BLOCK, -1);
                _load(block.data(), BLOCK_START + inode.indirect_block[i] * BLOCK_SIZE, BLOCK_SIZE);
                for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                    if (block[j] != -1)
                    {
                        // dout << "[读取 INode 块列表] 间接块 " << i << " ，地址 " << j << " ：" << block[j] << endl;
                        block_id_list.push_back(block[j]);
                    }
            }

        // 二级间接块
        for (int i = 0; i < NUM_DOUBLE_INDIRECT_BLOCK; i++)
            if (inode.double_indirect_block[i] != -1)
            {
                // dout << "[读取 INode 块列表] 双重间接块 " << i << " ：" << inode.double_indirect_block[i] << endl;
                vector<short> block(ADDRESS_PER_BLOCK, -1);
                _load(block.data(), BLOCK_START + inode.double_indirect_block[i] * BLOCK_SIZE, BLOCK_SIZE);
                for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                    if (block[j] != -1)
                    {
                        // dout << "[读取 INode 块列表] 双重间接块 " << i << " ，地址 " << j << " ：" << block[j] << endl;
                        vector<short> block2(ADDRESS_PER_BLOCK, -1);
                        _load(block2.data(), BLOCK_START + block[j] * BLOCK_SIZE, BLOCK_SIZE);
                        for (int k = 0; k < ADDRESS_PER_BLOCK; k++)
                            if (block2[k] != -1)
                                block_id_list.push_back(block2[k]);
                    }
            }

        return block_id_list;
    }

    vector<short> _get_block_list(const short &inode_id)
    {
        INode inode = _get_inode(inode_id);
        return _get_block_list(inode);
    }

    // 只存放间接地址块，有别于数据块
    vector<short> _get_addr_block_list(const INode &inode)
    {
        vector<short> indirect_block_list;

        // 间接块
        for (int i = 0; i < NUM_INDIRECT_BLOCK; i++)
            if (inode.indirect_block[i] != -1)
                indirect_block_list.push_back(inode.indirect_block[i]);

        // 二级间接块
        for (int i = 0; i < NUM_DOUBLE_INDIRECT_BLOCK; i++)
            if (inode.double_indirect_block[i] != -1)
            {
                // 将二级中的第一级间接块加入列表
                indirect_block_list.push_back(inode.double_indirect_block[i]);

                // 读取二级间接块
                vector<short> addresses(ADDRESS_PER_BLOCK, -1);
                _load(addresses.data(), BLOCK_START + inode.double_indirect_block[i] * BLOCK_SIZE, BLOCK_SIZE);
                dout << "[获取间接地址块列表] 二级间接块第一级所有地址：" << addresses << endl;
                for (const auto &address : addresses)
                    if (address != -1)
                        // 将二级中的第二级数据块加入列表
                        indirect_block_list.push_back(address);
            }

        dout << "[获取间接地址块列表] 间接地址块列表大小：" << indirect_block_list.size() << endl;
        dout << "[获取间接地址块列表] 间接地址块列表：" << indirect_block_list << endl;

        return indirect_block_list;
    }

    vector<short> _get_addr_block_list(const short &inode_id)
    {
        INode inode = _get_inode(inode_id);
        return _get_addr_block_list(inode);
    }

    string _absolute_path(const string path)
    {
        dout << "[获取绝对路径] 原始路径：" << path << endl;

        if (path.empty())
        {
            dout << "[获取绝对路径] 路径为当前工作目录：" << working_dir << endl;
            return working_dir;
        }

        // 如果是绝对路径，直接返回
        if (path[0] == '/')
        {
            dout << "[获取绝对路径] " << path << " 为绝对路径" << endl;
            return path;
        }

        // 如果 working_dir 末尾没有 '/'，则添加一个
        string absolute_path = working_dir;
        if (working_dir.back() != '/')
            absolute_path += "/";

        // 拼接当前工作目录
        absolute_path += path;

        // 若 absolute_path 末尾为 /.，则去掉
        if (absolute_path.length() >= 2 && absolute_path.substr(absolute_path.length() - 2) == "/.")
            absolute_path = absolute_path.substr(0, absolute_path.length() - 2);

        // 若中间有 /./，则去掉
        size_t pos = absolute_path.find("/./");
        while (pos != string::npos)
        {
            absolute_path = absolute_path.substr(0, pos) + absolute_path.substr(pos + 2);
            pos = absolute_path.find("/./");
        }

        // 若 absolute_path 末尾为 /..
        regex double_dot_pattern("/\\.\\.(/|$)");
        regex double_dot_valid_pattern("(/[^/]+?/\\.\\.)(/|$)");
        smatch double_dot_match, double_dot_valid_match;
        dout << "[获取绝对路径] 双点正则匹配结果：" << regex_search(absolute_path, double_dot_match, double_dot_pattern) << endl;
        // 若 absolute_path 含有 ..
        if (regex_search(absolute_path, double_dot_match, double_dot_pattern))
        {
            // 若 absolute_path 中的 .. 符合规范
            if (regex_search(absolute_path, double_dot_valid_match, double_dot_valid_pattern))
            {
                dout << "[获取绝对路径] double_dot_valid_match.size(): " << double_dot_valid_match.size() << endl;
                for (int i = 0; i < double_dot_valid_match.size(); i++)
                    dout << "[获取绝对路径] double_dot_valid_match[" << i << "]: " << double_dot_valid_match[i] << endl;
                dout << "[获取绝对路径] double_dot_valid_match.prefix(): " << double_dot_valid_match.prefix() << endl;
                dout << "[获取绝对路径] double_dot_valid_match.suffix(): " << double_dot_valid_match.suffix() << endl;
                dout << "[获取绝对路径] double_dot_valid_match.str(): " << double_dot_valid_match.str() << endl;
                dout << "[获取绝对路径] double_dot_valid_match.position(): " << double_dot_valid_match.position() << endl;

                // 将 absolute_path 中 double_dot_valid_match[1] 的部分替换为空
                absolute_path = regex_replace(absolute_path, double_dot_valid_pattern, "/");
            }
            else
            {
                dout << "[获取绝对路径] 路径" << absolute_path << " 不合法，返回空字符串" << endl;
                return "";
            }
        }

        if (absolute_path == "/..")
            return "/";

        // 若 absolute_path 末尾为 / ，则去掉
        if (absolute_path.back() == '/')
            absolute_path = absolute_path.substr(0, absolute_path.length() - 1);

        // 若 absolute_path 为空，则返回 /
        if (absolute_path.empty())
            return "/";

        return absolute_path;
    }

    // 将路径分割为路径向量
    vector<string> _split_path(const string path)
    {
        dout << "[路径分割] 原始路径：" << path << endl;
        string absolute_path = _absolute_path(path);
        dout << "[路径分割] 绝对路径：" << absolute_path << endl;

        // 分割路径
        istringstream iss(absolute_path);
        vector<string> dir_vector;
        string token;
        while (getline(iss, token, '/'))
            if (!token.empty())
                dir_vector.push_back(token);

        dout << "[路径分割] dir_vector: [";
        for (const auto &level : dir_vector)
            dout << level << " ";
        dout << "]" << endl;

        // 形式非法的路径将返回空值
        return dir_vector;
    }

    short _search_inode(const short &dir_inode_id, const string &filename)
    {
        dout << "[查找 Inode] 正在如下 INode 中查找目录项 " << filename << " ..." << endl;
        dout << _get_inode(dir_inode_id);

        vector<Dentry> dentry_list = _load_dentries(dir_inode_id);
        for (const auto &dentry : dentry_list)
            if (dentry.filename == filename)
            {
                dout << "[查找 Inode] 找到目录项 " << filename << "（inode_id: " << dentry.inode_id << "）" << endl;
                return dentry.inode_id;
            }

        return -1;
    }

    void _search_inode(const string path, short &dir_inode_id, short &file_inode_id)
    {
        dout << "[查找 Inode] 根据路径 " << path << " 查找 Inode ..." << endl;

        // 将路径转为路径向量
        vector<string> dir_vector = _split_path(path);

        dir_inode_id = -1;
        file_inode_id = -1;

        short ptr_inode_id = ROOT_INODE_ID;

        if (dir_vector.empty())
        {
            dout << "[查找 Inode] 路径为空，返回根目录 Inode ID：" << ptr_inode_id << endl;
            dir_inode_id = ptr_inode_id;
            file_inode_id = ptr_inode_id;
        }

        else
            for (const auto &level : dir_vector)
            {
                // 获取当前目录级别的数据块
                // dout << "[查找 Inode] 当前目录级别：" << level << endl;

                // 如果非最后一级的 level 不是目录
                if (_get_inode(ptr_inode_id).file_type != 'd' && &level != &dir_vector.back())
                {
                    dout << "[查找 Inode] 当前目录级别不是目录，查找失败！" << endl;
                    dir_inode_id = -1;
                    file_inode_id = -1;
                    return;
                }

                vector<Dentry> _dentry_list = _load_dentries(ptr_inode_id);

                // dout << "[查找 Inode] 当前目录项：" << endl;
                // for (const auto &dentry : _dentry_list)
                //     dout << dentry << endl;

                // 遍历目录项
                bool found = false;
                for (const auto &dentry : _dentry_list)
                    if (dentry.filename == level)
                    {
                        found = true;
                        dout << "[查找 Inode] 寻找到目录项 " << level << "（inode_id: " << dentry.inode_id << "）" << endl;

                        // 如果还不是最后一个目录项
                        if (&level != &dir_vector.back())
                        {
                            // 则需要继续往下找
                            ptr_inode_id = dentry.inode_id;
                            dout << "[查找 Inode] 继续向下一级寻找，ptr_inode_id: " << ptr_inode_id << endl;
                        }
                        else
                        {
                            // 否则找到了文件
                            dir_inode_id = ptr_inode_id;
                            file_inode_id = dentry.inode_id;
                            dout << "[查找 Inode] 找到了最终项 " << level << "，此时 dir_inode_id: " << dir_inode_id << "，file_inode_id: " << file_inode_id << endl;
                        }
                        break;
                    }

                if (!found)
                {
                    // 找到了次末级目录，但是没有找到最终项（文件/文件夹）
                    // 如果是最后一个，则说明文件不存在，否则说明目录不存在
                    if (&level == &dir_vector.back())
                    {
                        dir_inode_id = ptr_inode_id;
                        file_inode_id = -1;
                        dout << "[查找 Inode] 文件 " << level << " 不存在" << endl;
                    }
                    else
                    // 否则则是在中途找不到目录的情况
                    {
                        dir_inode_id = -1;
                        file_inode_id = -1;
                        dout << "[查找 Inode] 目录 " << level << " 不存在" << endl;
                    }
                    return;
                }
            }

        dout << "[查找 Inode] 根据路径查找 Inode 结果：" << endl;
        dout << "[查找 Inode] dir_inode_id: " << dir_inode_id << endl;
        dout << "[查找 Inode] file_inode_id: " << file_inode_id << endl;
    }

    string _filename(const string path)
    {
        vector<string> dir_vector = _split_path(path);
        if (dir_vector.empty())
            return "";
        else
        {
            dout << "文件名为 " << dir_vector.back() << endl;
            return dir_vector.back();
        }
    }

    void _add_dentry(INode &dir_inode, const short &new_inode_id, const string &filename)
    {
        dout << "[新增目录项] 正在向如下 INode 新增目录项 " << filename << "（INode ID 为" << new_inode_id << "）..." << endl;
        dout << dir_inode << endl;

        if (dir_inode.file_type != 'd')
        {
            dout << "[新增目录项] 该 INode 不是目录，新增目录项失败！" << endl;
            return;
        }

        // 获取当前目录的数据块
        vector<short> block_id_list = _get_block_list(dir_inode);
        dout << "[新增目录项] 该 INode 对应的 Dentry 数据块个数：" << block_id_list.size() << endl;

        // 遍历数据块，找到第一个空闲的 Dentry 位置
        bool found = false;
        for (const auto &block_id : block_id_list)
        {
            dout << "[新增目录项] 读取 Dentry 数据块 " << block_id << " ..." << endl;
            vector<Dentry> dentry_list(DENTRY_NUM_PER_BLOCK);
            _load(dentry_list.data(), BLOCK_START + block_id * BLOCK_SIZE, BLOCK_SIZE);

            for (int i = 0; i < DENTRY_NUM_PER_BLOCK; i++)
            {
                dout << "[新增目录项] Dentry 数据块 " << block_id << " 的第 " << i << " 个 Dentry：" << dentry_list[i].inode_id << "  " << dentry_list[i].get_filename() << endl;
                if (dentry_list[i].inode_id == -1)
                {
                    found = true;

                    // 设置 Dentry
                    dentry_list[i] = Dentry(new_inode_id, filename);
                    dout << "[新增目录项] Dentry 数据块 " << block_id << " 的第 " << i << " 个 Dentry 为空，已设置为 " << endl;
                    dout << dentry_list[i] << endl;
                    _dump(dentry_list.data(), BLOCK_START + block_id * BLOCK_SIZE, BLOCK_SIZE);

                    // INode 硬链接数加 1
                    INode _inode = _get_inode(new_inode_id);
                    _inode.link_cnt++;
                    _save_inode(_inode);

                    dout << "[新增目录项] 新增目录项成功！" << endl;
                    return;
                }
            }
        }

        if (!found)
        {
            dout << "[新增目录项] 该 INode 目前没有空闲的 Dentry 位置，新增 Dentry 块中 ..." << endl;
            // 遍历 INode 的直接块
            for (int i = 0; i < NUM_DIRECT_BLOCK; i++)
            {
                if (dir_inode.direct_block[i] == -1)
                {
                    dir_inode.direct_block[i] = _get_avail_block();
                    _create_blank_dentries(dir_inode.direct_block[i]);
                    dout << "[新增目录项] 直接块 " << i << " 为空，新增 Dentry 数据块：" << dir_inode.direct_block[i] << endl;
                    // INode 的文件大小增加一个数据块的大小
                    dir_inode.file_size += BLOCK_SIZE;
                    _save_inode(dir_inode);
                    _add_dentry(dir_inode, new_inode_id, filename);
                    return;
                }
            }
            // 遍历 INode 的间接块
            for (int i = 0; i < NUM_INDIRECT_BLOCK; i++)
            {
                if (dir_inode.indirect_block[i] == -1)
                {
                    // 申请新的地址数据块
                    dir_inode.indirect_block[i] = _get_avail_block();
                    dout << "[新增目录项] 间接块 " << i << " 为空，新增 Address 数据块：" << dir_inode.indirect_block[i] << endl;
                    // 初始化新的地址数据块
                    vector<short> addr(ADDRESS_PER_BLOCK, -1);
                    addr[0] = _get_avail_block();
                    // 初始化 Dentry 数据块
                    _create_blank_dentries(addr[0]);
                    dout << "[新增目录项] 间接块 " << i << " 的第 0 个地址为空，新增 Dentry 数据块：" << addr[0] << endl;
                    _dump(addr.data(), BLOCK_START + dir_inode.indirect_block[i] * BLOCK_SIZE, BLOCK_SIZE);
                    // INode 文件大小随着地址数据块的新增而增长
                    dir_inode.file_size += BLOCK_SIZE;
                    _save_inode(dir_inode);
                    _add_dentry(dir_inode, new_inode_id, filename);
                    return;
                }
                else
                {
                    dout << "[新增目录项] 间接块 " << i << " 不为空，读取 Address 数据块：" << dir_inode.indirect_block[i] << endl;
                    vector<short> addr(ADDRESS_PER_BLOCK);
                    _load(addr.data(), BLOCK_START + dir_inode.indirect_block[i] * BLOCK_SIZE, BLOCK_SIZE);
                    for (int j = 0; j < ADDRESS_PER_BLOCK; j++)
                    {
                        if (addr[j] == -1)
                        {
                            addr[j] = _get_avail_block();
                            _create_blank_dentries(addr[j]);
                            dout << "[新增目录项] 间接块 " << i << " 的第 " << j << " 个地址为空，新增 Dentry 数据块：" << addr[j] << endl;
                            _dump(addr.data(), BLOCK_START + dir_inode.indirect_block[i] * BLOCK_SIZE, BLOCK_SIZE);
                            dir_inode.file_size += BLOCK_SIZE;
                            _save_inode(dir_inode);
                            _add_dentry(dir_inode, new_inode_id, filename);
                            return;
                        }
                    }
                }
            }
        }
    };

    void _add_dentry(const short &dir_inode_id, const short &new_inode_id, const string &filename)
    {
        INode dir_inode = _get_inode(dir_inode_id);
        _add_dentry(dir_inode, new_inode_id, filename);
    }

    void _remove_dentry(const short &dir_inode_id, const short &inode_id)
    {
        vector<short> block_id_list = _get_block_list(dir_inode_id);
        vector<Dentry> dentry_list(DENTRY_NUM_PER_BLOCK);
        for (const auto &block_id : block_id_list)
        {
            _load(dentry_list.data(), BLOCK_START + block_id * BLOCK_SIZE, BLOCK_SIZE);
            for (auto &dentry : dentry_list)
                // 找到了需要删除的目录项
                if (dentry.inode_id == inode_id)
                {
                    dentry = Dentry();
                    _dump(dentry_list.data(), BLOCK_START + block_id * BLOCK_SIZE, BLOCK_SIZE);

                    // 文件自身硬链接数减一
                    INode inode = _get_inode(inode_id);
                    inode.link_cnt--;
                    _save_inode(inode);
                    dout << "[删除目录项] 目录项对应的 INode 硬链接数减 1：" << endl
                         << inode;

                    // 如果是目录，父目录的硬链接数减一
                    if (_get_inode(inode_id).file_type == 'd')
                    {
                        INode dir_inode = _get_inode(dir_inode_id);
                        dir_inode.link_cnt--;
                        _save_inode(dir_inode);
                        dout << "[删除目录项] 目录项为目录，其父目录的 INode 硬链接数减 1：" << endl
                             << dir_inode;
                    }

                    return;
                }
        }
    }

    vector<Dentry> _load_dentries(const INode &inode)
    {
        dout << "[读取目录项] 读取如下 INode 的目录项 ..." << endl;
        dout << inode << endl;

        if (inode.file_type != 'd')
        {
            dout << "[读取目录项] 该 INode 不是目录，读取失败！" << endl;
            return {};
        }

        vector<short> block_id_list = _get_block_list(inode);
        dout << "[读取目录项] 该 INode 对应的 Dentry 数据块：" << block_id_list << endl;

        vector<Dentry> dentry_list;
        for (const auto &block_id : block_id_list)
        {
            // dout << "[读取目录项] 读取 Dentry 数据块 " << block_id << " ..." << endl;
            vector<Dentry> dentry(DENTRY_NUM_PER_BLOCK);
            _load(dentry.data(), BLOCK_START + block_id * BLOCK_SIZE, BLOCK_SIZE);
            // dout << "[读取目录项] 读取 Dentry 数据块 " << block_id << " 结果：" << endl
            //      << dentry << endl;

            for (const auto &entry : dentry)
                if (entry.inode_id != -1)
                    dentry_list.push_back(entry);
        }

        // dout << "[读取目录项] 有效 Dentry：" << endl
        //      << dentry_list << endl;
        return dentry_list;
    }

    vector<Dentry> _load_dentries(short inode_id)
    {
        INode inode = _get_inode(inode_id);
        return _load_dentries(inode);
    }

    void _save_inode(const short &inode_id, const char &file_type, const int &file_size)
    {
        dout << "[保存 Inode] 保存如下 Inode ..." << endl;
        INode inode = INode();
        inode.id = inode_id;
        inode.file_type = file_type;
        inode.file_size = file_size;
        inode.create_time = Util::get_current_time();
        inode.modify_time = Util::get_current_time();
        dout << inode;
        _dump(&inode, INODE_TABLE_START + inode_id * INODE_SIZE, INODE_CLASS_SIZE);
    }

    void _save_inode(const INode &inode)
    {
        _dump(&inode, INODE_TABLE_START + inode.id * INODE_SIZE, INODE_CLASS_SIZE);
    }

    INode _get_inode(const short &inode_id)
    {
        INode inode = INode();
        _load(&inode, INODE_TABLE_START + inode_id * INODE_SIZE, INODE_CLASS_SIZE);
        assert(inode.id == inode_id);
        return inode;
    }

    const vector<short> _inode_cnt(const short &inode_id)
    {
        vector<short> cnt;
        const INode inode = _get_inode(inode_id);

        // 文件
        if (inode.file_type != 'd')
            cnt.push_back(inode.id);

        else if (inode.file_type == 'd')
        {
            cnt.push_back(inode.id);
            vector<Dentry> dentry_list = _load_dentries(inode);
            for (const auto &dentry : dentry_list)
                if (dentry.inode_id != -1 && dentry.get_filename() != "." && dentry.get_filename() != "..")
                {
                    vector<short> temp = _inode_cnt(dentry.inode_id);
                    cnt.insert(cnt.end(), temp.begin(), temp.end());
                }
        }

        return cnt;
    }

    const short inode_cnt(const short &inode_id)
    {
        dout << "[统计 Inode 数量] 正在统计 " << inode_id << " 的占用总 Inode 数 ..." << endl;
        vector<short> cnt = _inode_cnt(inode_id);
        dout << "[统计 Inode 数量] INode 列表: " << cnt << endl;
        unordered_set<short> unique_cnt(cnt.begin(), cnt.end());
        short sum = 0;
        for (const auto &id : unique_cnt)
            sum++;
        dout << "[统计 Inode 数量] INode 数量: " << sum << endl;
        return sum;
    }

    const short block_cnt(const short &inode_id)
    {
        dout << "[统计块数量] 正在统计 " << inode_id << " 的占用总块数 ..." << endl;

        short cnt = 0;
        vector<short> inode_list = _inode_cnt(inode_id);
        unordered_set<short> unique_inode_list(inode_list.begin(), inode_list.end());
        for (const auto &inode_id : unique_inode_list)
        {
            const INode inode = _get_inode(inode_id);
            // 计数加上数据块块数和地址块块数
            cnt += inode.file_size / BLOCK_SIZE + _get_addr_block_list(inode).size();
        }

        dout << "[统计块数量] INode " << inode_id << " 占用总块数：" << cnt << endl;
        return cnt;
    }

    string _load_file(const INode &inode)
    {
        dout << "[加载文件] 加载如下 INode 的文件内容 ..." << endl;
        dout << inode << endl;

        vector<short> block_id_list = _get_block_list(inode);

        dout << "[加载文件] 该文件有 " << block_id_list.size() << " 个数据块：" << endl;
        for (const auto &block_id : block_id_list)
            dout << block_id << " ";
        dout << endl;

        string file_data_str;
        file_data_str.reserve(inode.file_size);
        for (const auto &block_id : block_id_list)
        {
            vector<char> content(BLOCK_SIZE);
            _load(content.data(), BLOCK_START + block_id * BLOCK_SIZE, BLOCK_SIZE);
            // dout << "[加载文件] 读取数据块 " << block_id << " 内容：" << endl
            //      << content << endl;
            file_data_str.append(content.data(), BLOCK_SIZE);
            // unique_ptr<char[]> block(new char[BLOCK_SIZE]);
            // _load(block.get(), BLOCK_START + block_id * BLOCK_SIZE, BLOCK_SIZE);
            // file_data_str.append(block.get(), BLOCK_SIZE);
        }

        return file_data_str;
    }

    const short _create_file(const short &dir_inode_id, const string &filename, const int &filesize_kb)
    {
        short new_inode_id = _get_avail_inode();
        dout << "[创建文件] 已申请新 Inode：" << new_inode_id << endl;

        // Inode
        _save_inode(new_inode_id, 'f', filesize_kb * 1024);

        // 目录项
        _add_dentry(dir_inode_id, new_inode_id, filename);

        // 数据块
        vector<short> block_id_list;
        for (int i = 0; i < filesize_kb; i++)
        {
            short id = _get_avail_block();
            block_id_list.push_back(id);
            dout << "[创建文件] 已申请第 " << i << " 个块：" << id << endl;
        }
        _set_block_list(new_inode_id, block_id_list);

        // 向数据块写入随机内容
        srand(static_cast<unsigned int>(time(0)));
        for (const auto &id : block_id_list)
        {
            vector<char> content(BLOCK_SIZE);
            for (int i = 0; i < BLOCK_SIZE; i++)
                content[i] = 'a' + rand() % 26;
            // content[i] = '0' + i % 10;
            // dout << "[创建文件] 写入数据块 " << id << " 内容：" << endl
            //      << content << endl;
            _dump(content.data(), BLOCK_START + id * BLOCK_SIZE, BLOCK_SIZE);
        }

        return new_inode_id;
    }

    // 传入文件路径和文件大小（KB），创建文件
    void create_file(const string &path, const unsigned short &filesize_kb)
    {
        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        dout << "[创建文件] 准备创建 " << absolute_path << "，文件大小为 " << filesize_kb << "KB" << endl;

        // 根据路径查找 Inode
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);

        if (dir_inode_id == -1)
        {
            // cout << "[创建文件] 路径 " << absolute_path << " 无效" << endl;
            cout << "touch: cannot touch '" << absolute_path << "': No such file or directory" << endl;
            return;
        }

        // 如果 file_inode_id 不为 -1，则说明文件已存在
        if (file_inode_id != -1)
        {
            // cout << "[创建文件] 文件 " << absolute_path << " 已存在" << endl;
            cout << "touch: cannot touch '" << absolute_path << "': File exists" << endl;
            return;
        }

        if (superblock.available_inode_num == 0)
        {
            // cout << "[创建文件] 可用 Inode 不足，文件创建失败！" << endl;
            cout << "touch: cannot touch '" << absolute_path << "': No available inode" << endl;
            return;
        }

        if (superblock.available_block_num < Util::block_occupation(filesize_kb))
        {
            // cout << "[创建文件] 可用块不足，文件创建失败！创建大小为 " << filesize_kb << "KB 的文件需要 " << Util::block_occupation(filesize_kb) << " 个块，目前可用块剩余 " << superblock.available_block_num << " 个" << endl;
            cout << "touch: cannot touch '" << absolute_path << "': No available block" << endl;
            return;
        }

        short new_inode_id = _create_file(dir_inode_id, _filename(path), filesize_kb);

        dout << "[创建文件] 新 Inode 信息：" << endl;
        dout << _get_inode(new_inode_id) << endl;

        dout << "[创建文件] 文件 " << absolute_path << " 创建成功" << endl;
    }

    const short _create_dir(const short &dir_inode_id, const string &new_dirname)
    {
        // 申请新可用 Inode 和 Block
        short new_inode_id = _get_avail_inode();
        dout << "[创建目录] 已申请新 Inode：" << new_inode_id << endl;

        short new_block_id = _get_avail_block();
        dout << "[创建目录] 已申请新 Block：" << new_block_id << endl;

        // 初始化 Inode
        _save_inode(new_inode_id, 'd', BLOCK_SIZE);

        // 初始化 Dentry 数据块
        _create_blank_dentries(new_block_id, new_inode_id, dir_inode_id);

        // 向新文件所在的目录添加目录项
        _add_dentry(dir_inode_id, new_inode_id, new_dirname);

        return new_inode_id;
    }

    void create_dir(const string path, bool parent = false)
    {
        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        dout << "[创建目录] 准备创建 " << absolute_path << " ..." << endl;

        if (superblock.available_inode_num == 0)
        {
            // cout << "[创建目录] 可用 Inode 不足，目录创建失败！" << endl;
            cout << "mkdir: cannot create directory '" << absolute_path << "': No available inode" << endl;
            return;
        }

        if (superblock.available_block_num == 0)
        {
            dout << "[创建目录] 可用块不足，目录创建失败！此时的超级块信息：" << endl;
            dout << superblock;
            // cout << "[创建目录] 可用块不足，目录创建失败！" << endl;
            cout << "mkdir: cannot create directory '" << absolute_path << "': No available block" << endl;
            return;
        }

        // 根据路径查找 Inode
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);

        // 如果 file_inode_id 不为 -1，则说明文件已存在
        if (file_inode_id != -1)
        {
            // cout << "[创建目录] " << absolute_path << " 已存在" << endl;
            cout << "mkdir: cannot create directory '" << absolute_path << "': File exists" << endl;
            return;
        }

        short new_inode_id;
        if (!parent)
        {
            if (dir_inode_id == -1)
            {
                // cout << "[创建目录] 路径 " << absolute_path << " 无效" << endl;
                cout << "mkdir: cannot create directory '" << absolute_path << "': No such file or directory" << endl;
                return;
            }

            new_inode_id = _create_dir(dir_inode_id, _filename(path));
        }

        else if (parent)
        {
            vector<string> dir_vector = _split_path(absolute_path);
            short _dir_inode_id = ROOT_INODE_ID;
            short _ptr_inode_id;
            for (const auto &level : dir_vector)
            {
                _ptr_inode_id = _search_inode(_dir_inode_id, level);

                if (_ptr_inode_id == -1)
                    _ptr_inode_id = _create_dir(_dir_inode_id, level);

                _dir_inode_id = _ptr_inode_id;
            }
            new_inode_id = _ptr_inode_id;
        }

        dout << "[创建目录] 新 Inode 信息：" << endl;
        dout << _get_inode(new_inode_id) << endl;

        dout << "[创建目录] 目录 " << absolute_path << " 创建成功" << endl;
    }

    void _remove(const short &dir_inode_id, const short &file_inode_id)
    {
        // 删除特定文件
        if (file_inode_id > 0)
        {
            INode file_inode = _get_inode(file_inode_id);
            dout << "[删除文件] 正在删除如下文件：" << endl;
            dout << file_inode;

            // 删除该文件对应的目录项
            _remove_dentry(dir_inode_id, file_inode_id);

            // 如果文件自身硬链接数降为 0，则彻底删除文件
            if (_get_inode(file_inode_id).link_cnt == 0)
            {
                dout << "[删除文件] 文件硬链接数此时为 0，彻底删除文件 ..." << endl;

                // 释放文件的数据块
                vector<short> block_id_list = _get_block_list(file_inode);
                dout << "[删除文件] 删除该文件的数据块：" << block_id_list << endl;
                _clear_block(block_id_list);

                // 释放文件的间接地址块
                vector<short> addr_block_list = _get_addr_block_list(file_inode);
                dout << "[删除文件] 删除该文件的地址块：" << addr_block_list << endl;
                _clear_block(addr_block_list);

                // 释放文件的 INode
                dout << "[删除文件] 删除文件的 INode" << endl;
                _clear_inode(file_inode_id);
            }
            return;
        }

        else if (file_inode_id == -1)
        // 删除整个文件夹
        {
            // 列出所有目录项
            vector<Dentry> dentry_list = _load_dentries(dir_inode_id);
            for (const auto &dentry : dentry_list)
                if (dentry.inode_id != -1 && dentry.get_filename() != "." && dentry.get_filename() != "..")
                {
                    INode _inode = _get_inode(dentry.inode_id);
                    // 如果是文件
                    if (_inode.file_type == 'f')
                        _remove(dir_inode_id, dentry.inode_id);
                    // 如果是文件夹
                    if (_inode.file_type == 'd')
                        _remove(dentry.inode_id, -1);
                }

            // 此时已经是目录下为空
            dentry_list = _load_dentries(dir_inode_id);
            dout << "目录下的所有文件/文件夹已被递归删除，此时目录项为：" << endl;
            dout << dentry_list;

            // 删除该目录在其父目录中的目录项
            dout << "[删除目录] 删除该目录在其父目录中的目录项 ..." << endl;
            short parent_dir_inode_id = dentry_list[1].inode_id;
            _remove_dentry(parent_dir_inode_id, dir_inode_id);

            // 释放目录的数据块
            vector<short> block_id_list = _get_block_list(dir_inode_id);
            dout << "[删除目录] 删除该目录的数据块：" << block_id_list << endl;
            _clear_block(block_id_list);

            // 释放目录的间接地址块
            vector<short> addr_block_list = _get_addr_block_list(dir_inode_id);
            dout << "[删除目录] 删除该目录的地址块：" << addr_block_list << endl;
            _clear_block(addr_block_list);

            // 释放目录的 INode
            dout << "[删除目录] 删除目录的 INode" << endl;
            _clear_inode(dir_inode_id);
        }
    }

    void remove(const string &path, bool recursive = false)
    {
        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        dout << "[删除文件/目录] 准备删除 " << absolute_path << " ..." << endl;

        if (absolute_path == "/")
        {
            // cout << "[删除文件/目录] 根目录不可删除" << endl;
            cout << "rm: cannot remove root directory" << endl;
            return;
        }

        if (absolute_path == working_dir)
        {
            // cout << "[删除文件/目录] 当前工作目录不可删除" << endl;
            cout << "rm: cannot remove current working directory" << endl;
            return;
        }

        if (path == "." || path == ".." || Util::ends_with(path, "/.") || Util::ends_with(path, "/.."))
        {
            // cout << "[删除文件/目录] 不可删除 '.' 或 '..'" << endl;
            cout << "rm: cannot remove '.' or '..'" << endl;
            return;
        }

        // 根据路径查找 Inode
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);

        if (file_inode_id == -1)
        {
            // cout << "[删除文件/目录] 文件 " << absolute_path << " 不存在" << endl;
            cout << "rm: cannot remove '" << absolute_path << "': No such file or directory" << endl;
            return;
        }

        const INode file_inode = _get_inode(file_inode_id);

        if (file_inode.file_type == 'f')
        {
            dout << "[删除文件/目录] 准备删除文件 " << absolute_path << " ..." << endl;
            _remove(dir_inode_id, file_inode_id);
            dout << "[删除文件/目录] 文件 " << absolute_path << " 已删除" << endl;
        }

        else if (file_inode.file_type == 'd')
        {
            if (!recursive)
            {
                // cout << "[删除文件/目录] 无法删除目录 " << absolute_path << "，如需删除，请使用 rm -r" << endl;
                cout << "rm: cannot remove '" << absolute_path << "': Is a directory" << endl;
                cout << "rm: use 'rm -r' to remove a directory" << endl;
                return;
            }
            else if (recursive)
            {
                dout << "[删除文件/目录] 准备删除目录 " << absolute_path << " ..." << endl;
                _remove(file_inode_id, -1);
                dout << "[删除文件/目录] 目录 " << absolute_path << " 已删除" << endl;
            }
        }
    }

    void _copy(const short &src_inode_id, const short &dst_dir_inode_id, const string &dst_filename)
    {
        // 假设已经完成了一切检查，此函数仅作执行操作
        const INode src_inode = _get_inode(src_inode_id);
        dout << "[复制文件/目录] 正在复制文件/目录 " << src_inode_id << " 到目录 " << dst_dir_inode_id << "，目标文件名为 " << dst_filename << " ..." << endl;
        dout << "[复制文件/目录] 源文件/目录信息：" << endl;
        dout << src_inode;
        dout << "[复制文件/目录] 目标目录信息：" << endl;
        dout << _get_inode(dst_dir_inode_id);

        if (src_inode.file_type == 'f')
        {
            // 新建指定名称的文件
            short new_inode_id = _create_file(dst_dir_inode_id, dst_filename, src_inode.file_size / 1024);

            // 复制数据块
            vector<short> src_block_id_list = _get_block_list(src_inode_id);
            vector<short> dst_block_id_list = _get_block_list(new_inode_id);
            for (int i = 0; i < src_block_id_list.size(); i++)
            {
                vector<char> content(BLOCK_SIZE);
                _load(content.data(), BLOCK_START + src_block_id_list[i] * BLOCK_SIZE, BLOCK_SIZE);
                _dump(content.data(), BLOCK_START + dst_block_id_list[i] * BLOCK_SIZE, BLOCK_SIZE);
            }
        }
        else if (src_inode.file_type == 'd')
        {
            dout << "[复制文件/目录] 准备迭代复制目录下的文件/目录 ..." << endl;

            // 新建指定名称的目录
            short new_inode_id = _create_dir(dst_dir_inode_id, dst_filename);

            // 递归复制源文件夹下的文件
            vector<Dentry> dentry_list = _load_dentries(src_inode_id);
            for (const auto &dentry : dentry_list)
                if (dentry.inode_id != -1 && dentry.get_filename() != "." && dentry.get_filename() != "..")
                    _copy(dentry.inode_id, new_inode_id, dentry.get_filename());
        }
    }

    void copy(const string &src_path, const string &dst_path, bool recursive = false)
    {
        // 将路径转为绝对路径
        string absolute_src_path = _absolute_path(src_path);
        string absolute_dst_path = _absolute_path(dst_path);

        dout << "[复制文件/目录] 准备复制 " << absolute_src_path << " 到 " << absolute_dst_path << " ..." << endl;

        // 根据路径查找 Inode
        short src_dir_inode_id, src_file_inode_id;
        _search_inode(src_path, src_dir_inode_id, src_file_inode_id);

        short dst_dir_inode_id, dst_file_inode_id;
        _search_inode(dst_path, dst_dir_inode_id, dst_file_inode_id);

        if (src_file_inode_id == -1)
        {
            // cout << "[复制文件/目录] 源文件/目录 " << absolute_src_path << " 不存在" << endl;
            cout << "cp: cannot copy '" << absolute_src_path << "': No such file or directory" << endl;
            return;
        }

        const INode src_file_inode = _get_inode(src_file_inode_id);

        if (dst_dir_inode_id == -1)
        {
            // cout << "[复制文件/目录] 目标路径 " << absolute_dst_path << " 有误" << endl;
            cout << "cp: cannot copy into '" << absolute_dst_path << "': No such file or directory" << endl;
            return;
        }

        if (dst_file_inode_id > 0 && _get_inode(dst_file_inode_id).file_type == 'f')
        {
            dout << "[复制文件/目录] 目标文件/目录 " << absolute_dst_path << " 已存在，其 INode 如下：" << endl;
            dout << _get_inode(dst_file_inode_id);
            cout << "cp: cannot copy into '" << absolute_dst_path << "': File exists" << endl;
            return;
        }

        const short src_inode_cnt = inode_cnt(src_file_inode_id);

        if (superblock.available_inode_num < src_inode_cnt)
        {
            dout << "[复制文件/目录] 可用 Inode 不足，复制失败！源文件/目录占用 " << src_inode_cnt << " 个 Inode，目前可用 Inode 剩余 " << superblock.available_inode_num << " 个" << endl;
            cout << "cp: cannot copy '" << absolute_src_path << "': No available inode" << endl;
            return;
        }

        const short src_block_cnt = block_cnt(src_file_inode_id);

        if (superblock.available_block_num < src_block_cnt)
        {
            dout << "[复制文件/目录] 可用块不足，复制失败！源文件/目录占用 " << src_block_cnt << " 个块，目前可用块剩余 " << superblock.available_block_num << " 个" << endl;
            cout << "cp: cannot copy '" << absolute_src_path << "': No available block" << endl;
            return;
        }

        if (src_file_inode.file_type == 'f')
        {
            if (dst_file_inode_id == -1)
            {
                dout << "[复制文件/目录] 准备将源文件 " << absolute_src_path << " 复制到目标路径 " << absolute_dst_path << " ..." << endl;
                _copy(src_file_inode_id, dst_dir_inode_id, _filename(dst_path));
            }

            else if (_get_inode(dst_file_inode_id).file_type == 'd')
            {
                dout << "[复制文件/目录] 准备将源文件 " << absolute_src_path << " 复制到目标路径 " << absolute_dst_path + "/" + _filename(src_path) << " ..." << endl;
                _copy(src_file_inode_id, dst_file_inode_id, _filename(src_path));
            }
        }

        else if (src_file_inode.file_type == 'd')
        {
            if (!recursive)
            {
                // cout << "[复制文件/目录] 无法复制目录 " << absolute_src_path << "，如需复制，请使用 cp -r" << endl;
                cout << "cp: cannot copy '" << absolute_src_path << "': Is a directory" << endl;
                cout << "cp: use 'cp -r' to copy a directory" << endl;
                return;
            }
            else if (recursive)
            {
                if (dst_file_inode_id == -1)
                {
                    dout << "[复制文件/目录] 准备将源文件 " << absolute_src_path << " 复制到目标路径 " << absolute_dst_path << " ..." << endl;
                    _copy(src_file_inode_id, dst_dir_inode_id, _filename(dst_path));
                }

                else if (_get_inode(dst_file_inode_id).file_type == 'd')
                {
                    dout << "[复制文件/目录] 准备将源文件 " << absolute_src_path << " 复制到目标路径 " << absolute_dst_path + "/" + _filename(src_path) << " ..." << endl;
                    _copy(src_file_inode_id, dst_file_inode_id, _filename(src_path));
                }
            }
        }

        dout << "[复制文件/目录] 文件/目录 " << absolute_src_path << " 已成功复制到 " << absolute_dst_path << endl;
    }

    void hard_link(const string &src_path, const string &dst_path)
    {
        // 将路径转为绝对路径
        string absolute_src_path = _absolute_path(src_path);
        string absolute_dst_path = _absolute_path(dst_path);

        dout << "[硬链接] 准备在 " << absolute_dst_path << " 创建硬链接，指向 " << absolute_src_path << " ..." << endl;

        // 根据路径查找 Inode
        short src_dir_inode_id, src_file_inode_id;
        _search_inode(src_path, src_dir_inode_id, src_file_inode_id);

        short dst_dir_inode_id, dst_file_inode_id;
        _search_inode(dst_path, dst_dir_inode_id, dst_file_inode_id);

        if (src_file_inode_id == -1)
        {
            // cout << "[硬链接] 源文件 " << absolute_src_path << " 不存在" << endl;
            cout << "ln: cannot create link '" << absolute_dst_path << "': No such file or directory" << endl;
            return;
        }

        if (dst_file_inode_id != -1)
        {
            // cout << "[硬链接] 目标文件 " << absolute_dst_path << " 已存在" << endl;
            cout << "ln: cannot create link '" << absolute_dst_path << "': File exists" << endl;
            return;
        }

        // 创建硬链接（仅新增目录项，不改动 INode 和数据块、地址块）
        _add_dentry(dst_dir_inode_id, src_file_inode_id, _filename(dst_path));
        dout << "[硬链接] 硬链接 " << absolute_dst_path << " 已创建" << endl;
    }

    // 读取并打印文件内容
    void cat(const string &path)
    {
        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        // 根据路径查找 Inode
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);

        // 如果 file_inode_id 为 -1，则说明文件不存在
        if (file_inode_id == -1)
        {
            // cout << "[查看文件内容] 文件 " << absolute_path << " 不存在" << endl;
            cout << "cat: " << absolute_path << ": No such file or directory" << endl;
            return;
        }

        // 获取文件 INode
        INode file_inode = _get_inode(file_inode_id);

        // 如果不是文件
        if (file_inode.file_type != 'f')
        {
            // cout << "[查看文件内容] " << absolute_path << " 不是文件" << endl;
            cout << "cat: " << absolute_path << ": Is a directory" << endl;
            return;
        }

        // 读取文件内容
        string content = _load_file(file_inode);
        // cout << "[查看文件内容] 文件 " << absolute_path << " 内容如下：" << endl;
        cout << content << endl;
        // cout << "-------------------------" << endl;
    }

    // 改变当前工作目录
    void change_dir(string path)
    {
        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        if (absolute_path.empty())
        {
            // cout << "[切换工作目录] 路径 " << path << " 不合法" << endl;
            cout << "cd: invalid path" << endl;
            return;
        }

        // 根据路径查找 Inode
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);
        short _inode_id = file_inode_id;

        // 如果 dir_inode_id 为 -1，则说明目录不存在
        if (_inode_id == -1)
        {
            // cout << "[切换工作目录] " << absolute_path << " 不存在" << endl;
            cout << "cd: " << absolute_path << ": No such file or directory" << endl;
            return;
        }
        else
        {
            INode dir_inode = _get_inode(_inode_id);
            if (dir_inode.file_type != 'd')
            {
                // cout << "[切换工作目录] " << absolute_path << " 不是目录" << endl;
                cout << "cd: " << absolute_path << ": Not a directory" << endl;
                return;
            }
            else
            {
                dout << "[切换工作目录] 当前工作目录：" << working_dir << " -> " << absolute_path << endl;
                working_dir = absolute_path;
                working_dir_inode_id = _inode_id;
            }
        }
    }

    void list_dir(string path = "")
    {
        short _inode_id, dir_inode_id;

        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        if (path.empty())
            _inode_id = working_dir_inode_id;
        else if (path == "/")
            _inode_id = ROOT_INODE_ID;
        else
            _search_inode(path, dir_inode_id, _inode_id);

        // 如果 dir_inode_id 为 -1，则说明目录不存在
        if (_inode_id == -1)
        {
            // cout << "[列出目录] " << absolute_path << " 不存在" << endl;
            cout << "ls: cannot access '" << absolute_path << "': No such file or directory" << endl;
            return;
        }
        else
        {
            INode _inode = _get_inode(_inode_id);
            if (_inode.file_type != 'd')
            {
                // cout << "[列出目录] " << absolute_path << " 不是目录" << endl;
                cout << "ls: cannot access '" << absolute_path << "': Not a directory" << endl;
                return;
            }
            else
            {
                vector<Dentry> dentry_list = _load_dentries(_inode);
                // cout << "[列出目录] 目录 " << absolute_path << " 内容如下：" << endl;
                for (const auto &dentry : dentry_list)
                {
                    if (dentry.inode_id != -1)
                    {
                        INode inode = _get_inode(dentry.inode_id);
                        cout << setw(4) << inode.id << "  ";
                        if (inode.file_type == 'd')
                            // cout << "目录 ";
                            cout << " dir ";
                        else if (inode.file_type == 'f')
                            // cout << "文件 ";
                            cout << "file ";
                        else
                            // cout << "未知 ";
                            cout << "unkn ";
                        cout << setw(2) << inode.link_cnt << "  ";
                        cout << setw(4) << Util::readable_size(inode.file_size) << "  ";
                        cout << Util::time_to_string(inode.create_time) << " ";
                        cout << Util::time_to_string(inode.modify_time) << "  ";
                        // 文件名
                        if (inode.file_type == 'd')
                            cout << BOLD << BLUE << dentry.get_filename() << RESET << endl;
                        else
                            cout << dentry.get_filename() << endl;
                    }
                }
                // cout << "------------------------------------------" << endl;
            }
        }
    }

    void stat(const string &path)
    {
        // 将路径转为绝对路径
        string absolute_path = _absolute_path(path);

        // 根据路径查找 Inode
        short dir_inode_id, file_inode_id;
        _search_inode(path, dir_inode_id, file_inode_id);

        // 如果 file_inode_id 为 -1，则说明文件不存在
        if (file_inode_id == -1)
        {
            // cout << "[查看文件状态] 文件 " << absolute_path << " 不存在" << endl;
            cout << "stat: cannot stat '" << absolute_path << "': No such file or directory" << endl;
            return;
        }

        // 打印 INode 信息
        cout << "File: " << absolute_path << endl;
        cout << _get_inode(file_inode_id);
    }

    void sum()
    {
        cout << superblock;
        // FileSystem::_show_macros();
    }

    // 打印 bitmap
    void _show_bitmap()
    {
        dout << "Block Bitmap: " << endl;
        for (int i = 0; i < BLOCK_BITMAP_SIZE; i++)
            dout << (int)block_bitmap.bitmap[i];
        dout << endl;

        vector<short> block_list;
        for (int i = 0; i < DATA_BLOCK_NUM; i++)
            if (block_bitmap.get(i))
                block_list.push_back(i);
        dout << "已分配的块列表：" << block_list << endl;

        dout << "INode Bitmap:" << endl;
        for (int i = 0; i < INODE_BITMAP_SIZE; i++)
            dout << (int)inode_bitmap.bitmap[i];
        dout << endl;

        vector<short> inode_list;
        for (int i = 0; i < INODE_NUM; i++)
            if (inode_bitmap.get(i))
                inode_list.push_back(i);
        dout << "已分配的 Inode 列表：" << inode_list << endl;
    }

    void _vaildate_bitmap_consistency()
    {
        // 计算 bitmap 上 1 的总个数
        int block_bitmap_sum = 0;
        int inode_bitmap_sum = 0;
        for (int i = 0; i < DATA_BLOCK_NUM; i++)
            block_bitmap_sum += block_bitmap.get(i);
        for (int i = 0; i < INODE_NUM; i++)
            inode_bitmap_sum += inode_bitmap.get(i);

        dout << "block_bitmap_sum: " << block_bitmap_sum << " " << 516 + block_bitmap_sum << endl;
        dout << "inode_bitmap_sum: " << inode_bitmap_sum << endl;

        dout << superblock;
    }

    // 打印所有的宏定义
    static void _show_macros()
    {
        const map<string, int> FILESYS_INFO = {
            {"FILESYSTEM_SIZE", FILESYSTEM_SIZE},
            {"BLOCK_SIZE", BLOCK_SIZE},
            {"INODE_SIZE", INODE_SIZE},
            {"BLOCK_NUM", BLOCK_NUM},
            {"INODE_NUM", INODE_NUM},
            {"MAX_FILE_SIZE", MAX_FILE_SIZE},
            {"MAX_FILENAME_SIZE", MAX_FILENAME_SIZE},
            {"DENTRY_SIZE", DENTRY_SIZE},
            {"DENTRY_NUM_PER_BLOCK", DENTRY_NUM_PER_BLOCK},
            {"SUPERBLOCK_SIZE", SUPERBLOCK_SIZE},
            {"BLOCK_BITMAP_SIZE", BLOCK_BITMAP_SIZE},
            {"INODE_BITMAP_SIZE", INODE_BITMAP_SIZE},
            {"INODE_TABLE_SIZE", INODE_TABLE_SIZE},
            {"SUPERBLOCK_START", SUPERBLOCK_START},
            {"BLOCK_BITMAP_START", BLOCK_BITMAP_START},
            {"INODE_BITMAP_START", INODE_BITMAP_START},
            {"INODE_TABLE_START", INODE_TABLE_START},
            {"BLOCK_START", BLOCK_START},
            {"DATA_BLOCK_NUM", DATA_BLOCK_NUM},
            {"ADDRESS_SIZE", ADDRESS_SIZE},
            {"ADDRESS_PER_BLOCK", ADDRESS_PER_BLOCK},
            {"NUM_DIRECT_BLOCK", NUM_DIRECT_BLOCK},
            {"NUM_INDIRECT_BLOCK", NUM_INDIRECT_BLOCK},
            {"NUM_DOUBLE_INDIRECT_BLOCK", NUM_DOUBLE_INDIRECT_BLOCK},
            {"ROOT_INODE_ID", ROOT_INODE_ID}};

        // cout << "----------- 文件系统预定义常量 ------------" << endl;
        cout << "---- Filesystem Predefined Constants ----" << endl;
        for (const auto &pair : FILESYS_INFO)
        {
            Util::cout_center(pair.first, 30);
            cout << pair.second << endl;
        }
        cout << "------------------------------------------" << endl;
    }
};

namespace plog
{
    class PlainFomatter
    {
    public:
        // This method returns a header for a new file. In our case it is empty.
        static util::nstring header()
        {
            return util::nstring();
        }

        // This method returns a string from a record.
        static util::nstring format(const Record &record)
        {
            util::nostringstream ss;
            ss << record.getMessage();
            return ss.str();
        }
    };
}

// static plog::ConsoleAppender<plog::MessageOnlyFormatter> consoleAppender;
// plog::init<Console>(plog::info, &consoleAppender);

int main(int argc, char *argv[])
{
    system("clear");

    if (argc > 1)
    {
        string param = string(argv[1]);
        if (param == "d" || param == "debug")
        {
            static plog::ConsoleAppender<plog::PlainFomatter> consoleAppender;
            plog::init(plog::debug, &consoleAppender);
            // static plog::RollingFileAppender<plog::PlainFomatter> fileAppender("main.log");
            // plog::init(plog::debug, &fileAppender);
        }
    }

    // 欢迎界面
    cout << R"(

    ███████     █████████      JIANG Ying-Jin（江英进）
  ███░░░░░███  ███░░░░░███         202130430089     
 ███     ░░███░███    ░░░                   
░███      ░███░░█████████      LI Jinyi（李晋一）                         
░███      ░███ ░░░░░░░░███         202130430119      
░░███     ███  ███    ░███                 
 ░░░███████░  ░░█████████      LI Lulong（李宇龙）                        
   ░░░░░░░     ░░░░░░░░░           202130430126                     
                                                        
 ██████████                     ███                     
░░███░░░░███                   ░░░                      
 ░███   ░░███  ██████   █████  ████   ███████ ████████  
 ░███    ░███ ███░░███ ███░░  ░░███  ███░░███░░███░░███ 
 ░███    ░███░███████ ░░█████  ░███ ░███ ░███ ░███ ░███ 
 ░███    ███ ░███░░░   ░░░░███ ░███ ░███ ░███ ░███ ░███ 
 ██████████  ░░██████  ██████  █████░░███████ ████ █████
░░░░░░░░░░    ░░░░░░  ░░░░░░  ░░░░░  ░░░░░███░░░░ ░░░░░ 
                                     ███ ░███           
 Operating System Design Project    ░░██████            
  Advised by Prof. ZHONG Jinghui     ░░░░░░             

    )" << endl;

    // 初始化文件系统
    static FileSystem fs;

    string user_input;
    vector<string> input_vec;

    // 注册 Ctrl+C 信号处理函数
    signal(SIGINT, [](int sig_num)
           {
        cout << endl << "SIGINT captured. Exiting gracefully." << endl;
        exit(0); });

    while (true)
    {
        cout << BOLD << CYAN << fs.working_dir << GREEN << " > " << RESET;
        getline(cin, user_input);

        // 去除前后空格并分割
        user_input = Util::trim_space(user_input);
        input_vec = Util::split_space(user_input);
        // boost::trim_space(user_input);
        // boost::split_space(input_vec, user_input, boost::is_space(), boost::token_compress_on);

        dout << "用户输入：" << user_input << "，分割结果：" << input_vec << "，长度：" << input_vec.size() << "，为空：" << input_vec.empty() << endl;

        if (!input_vec.empty())
        {
            // l / ls
            if (input_vec[0] == "l" || input_vec[0] == "ls")
                fs.list_dir();

            // cd
            else if (input_vec[0] == "cd")
            {
                if (input_vec.size() < 2)
                    ;
                else if (input_vec.size() > 2)
                    cout << "cd: too many arguments" << endl;
                else
                    fs.change_dir(input_vec[1]);
            }

            // sum
            else if (input_vec[0] == "sum")
                fs.sum();

            // cat
            else if (input_vec[0] == "cat")
            {
                if (input_vec.size() != 2)
                    cout << "cat: invalid arguments" << endl
                         << "Usage: cat [filename]" << endl;
                else
                    fs.cat(input_vec[1]);
            }

            // touch
            else if (input_vec[0] == "touch" || input_vec[0] == "createFile")
            {
                if (input_vec.size() != 3)
                    cout << input_vec[0] << ": invalid arguments" << endl
                         << "Usage: touch [filename] [filesize_kb]" << endl;
                else
                {
                    try
                    {
                        const short filesize_kb = stoi(input_vec[2]);
                        if (filesize_kb < 0)
                            cout << input_vec[0] << ": invalid filesize" << endl
                                 << "Usage: touch [filename] [filesize_kb]" << endl;
                        else
                            fs.create_file(input_vec[1], filesize_kb);
                    }
                    catch (const exception &e)
                    {
                        cout << input_vec[0] << ": invalid filesize" << endl
                             << "Usage: touch [filename] [filesize_kb]" << endl;
                    }
                }
            }

            // mkdir
            else if (input_vec[0] == "mkdir" || input_vec[0] == "createDir")
            {
                if (input_vec.size() < 2)
                    // 可以允许多个 dirname 参数
                    cout << input_vec[0] << ": invalid arguments" << endl
                         << "Usage: mkdir [dirname1] [dirname2] ..." << endl;
                else
                    for (int i = 1; i < input_vec.size(); i++)
                        fs.create_dir(input_vec[i], 1);
            }

            // rm
            else if (input_vec[0] == "rm")
            {
                // 允许多个文件/目录参数，能够检测 -r 参数
                if (input_vec.size() < 2)
                    cout << input_vec[0] << ": invalid arguments" << endl
                         << "Usage: rm [-r] [filename1] [filename2] ..." << endl;
                else
                {
                    bool recursive = false;
                    for (const auto &input : input_vec)
                        if (input[0] == '-' && input.find("r") != string::npos)
                        {
                            recursive = true;
                            input_vec.erase(remove(input_vec.begin(), input_vec.end(), input), input_vec.end());
                        }

                    for (int i = 1; i < input_vec.size(); i++)
                        fs.remove(input_vec[i], recursive);
                }
            }

            // cp
            else if (input_vec[0] == "cp")
            {
                // 允许多个文件/目录参数，能够检测 -r 参数
                if (input_vec.size() < 3)
                    cout << input_vec[0] << ": invalid arguments" << endl
                         << "Usage: cp [-r] [src] [dst]" << endl;
                else
                {
                    bool recursive = false;
                    for (const auto &input : input_vec)
                        // if (input[0] == '-' && boost::algorithm::contains(input, "r"))
                        if (input[0] == '-' && input.find("r") != string::npos)
                        {
                            recursive = true;
                            input_vec.erase(remove(input_vec.begin(), input_vec.end(), input), input_vec.end());
                        }

                    fs.copy(input_vec[1], input_vec[2], recursive);
                }
            }

            // ln
            else if (input_vec[0] == "ln")
            {
                if (input_vec.size() != 3)
                    cout << input_vec[0] << ": invalid arguments" << endl
                         << "Usage: ln [src] [dst]" << endl;
                else
                    fs.hard_link(input_vec[1], input_vec[2]);
            }

            // stat
            else if (input_vec[0] == "stat")
            {
                if (input_vec.size() != 2)
                    cout << input_vec[0] << ": invalid arguments" << endl
                         << "Usage: stat [filename]" << endl;
                else
                    fs.stat(input_vec[1]);
            }

            // sum
            else if (input_vec[0] == "sum")
                fs.sum();

            // bitmap / bm
            else if (input_vec[0] == "bitmap" || input_vec[0] == "bm")
                fs._show_bitmap();

            // exit
            else if (input_vec[0] == "exit")
                break;

            else if (input_vec[0] == "clear")
                system("clear");

            // erase
            else if (input_vec[0] == "erase")
            {
                // 保存原始 cout 缓冲区
                streambuf *cout_sbuf = cout.rdbuf();
                // 将 cout 重定向到 /dev/null
                ofstream devnull("/dev/null");
                cout.rdbuf(devnull.rdbuf());

                system("rm file.sys");
                fs = FileSystem();

                // 恢复 cout 到原始缓冲区
                cout.rdbuf(cout_sbuf);
                cout << "[Erase] Filesystem erased" << endl;
            }

            // cmd
            else if (input_vec[0] == "cmd")
            {
                cout << "Available commands:" << endl;
                cout << "\tl / ls" << endl
                     << "\t\tList files in current directory" << endl;
                cout << "\tcd [path]" << endl
                     << "\t\tChange working directory" << endl;
                cout << "\tsum" << endl
                     << "\t\tShow filesystem summary" << endl;
                cout << "\tcat [filename]" << endl
                     << "\t\tShow file content" << endl;
                cout << "\ttouch [filename] [filesize_kb]" << endl
                     << "\t\tCreate a file" << endl;
                cout << "\tmkdir [dirname1] [dirname2] ..." << endl
                     << "\t\tCreate a directory" << endl;
                cout << "\trm [-r] [filename1] [filename2] ..." << endl
                     << "\t\tRemove a file or directory" << endl;
                cout << "\tcp [-r] [src] [dst]" << endl
                     << "\t\tCopy a file or directory" << endl;
                cout << "\tln [src] [dst]" << endl
                     << "\t\tCreate a hard link" << endl;
                cout << "\tstat [filename]" << endl
                     << "\t\tShow file INode info" << endl;
                cout << "\tsum" << endl
                     << "\t\tShow filesystem summary" << endl;
                cout << "\tclear" << endl
                     << "\t\tClear screen" << endl;
                cout << "\terase" << endl
                     << "\t\tErase filesystem" << endl;
                cout << "\texit" << endl
                     << "\t\tExit" << endl;
            }

            else if (input_vec[0] == "")
                ;

            else
                cout << "command not found: " << input_vec[0] << endl
                     << "Type 'cmd' to see all available commands" << endl;
        }
    }

    return 0;
}
