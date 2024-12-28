# Unix-like File System

A lightweight, Unix-like file system implemented in memory, designed to simulate real-world file system operations. This project provides a practical demonstration of file system design concepts, including file and directory management, virtual addressing, and storage efficiency.

The system supports common file system operations such as file creation, deletion, directory management, and storage space monitoring. It also includes advanced features like indirect addressing and hard links, making it an excellent learning tool for understanding the inner workings of file systems.

---

## Features

- **In-Memory File System**: Allocates 16MB of memory as the storage space, divided into 1KB blocks.
- **Virtual Addressing**: Supports 24-bit virtual addresses with a well-designed address structure.
- **Efficient Inode Design**: Each inode supports 10 direct block addresses and one indirect block address.
- **Comprehensive File Operations**: Includes file creation, deletion, directory management, and file content display.
- **Advanced Features**: Supports secondary indirect addressing and hard links for enhanced functionality.
- **Persistent Storage**: Memory content is saved to disk upon exit for future reloading.

---

## Quick Start

### Run with Docker (Recommended)

For a seamless experience, it's recommended to run the project using Docker.

**First-time setup:**

```bash
docker run -it --name fs csjiangyj/filesys
```

**Restart the container after exiting:**

```bash
docker start fs && docker exec -it fs fs
```

---

## System Requirements & Design

This project simulates a Unix-like file system with the following specifications:

1. **Memory Allocation**:

   - Allocates 16MB of memory as the file system's storage space.
   - Storage is divided into 1KB blocks.
   - Virtual address length is 24 bits, with a carefully designed virtual address structure.

2. **Inode Design**:

   - Inodes store metadata for files and directories.
   - Each inode supports 10 direct block addresses and one indirect block address.
   - The first inode is reserved for the root directory (`/`).

3. **File Content**:

   - Files are filled with random strings for simplicity.
   - File creation requires specifying the file size (in KB) and its path.

4. **Command Support**:  
   The system implements the following commands:

   - **Welcome Message**: Displays group information (name and ID) upon startup, fulfilling copyright requirements.
   - **File Operations**:
     - `createFile <filename> <size>`: Creates a file with the specified size (in KB).  
       Example: `createFile /dir1/myFile 10`  
       If the size exceeds the maximum file size, an error message is displayed.
     - `deleteFile <filename>`: Deletes the specified file.  
       Example: `deleteFile /dir1/myFile`
   - **Directory Operations**:
     - `createDir <directory>`: Creates a directory (supports nested directories).  
       Example: `createDir /dir1/sub1`
     - `deleteDir <directory>`: Deletes a directory (current working directory cannot be deleted).  
       Example: `deleteDir /dir1/sub1`
     - `changeDir <directory>`: Changes the current working directory.  
       Example: `changeDir /dir2`
   - **Directory Listing**:
     - `dir`: Lists all files and subdirectories in the current directory, along with at least two file attributes (e.g., file size, creation time).
   - **File Copy**:
     - `cp <source> <destination>`: Copies a file to a new location.  
       Example: `cp /dir1/file1 /dir2/file2`
   - **Storage Monitoring**:
     - `sum`: Displays the usage of the 16MB storage space, including used and free blocks.
   - **File Content Display**:
     - `cat <filename>`: Prints the content of a file to the terminal.  
       Example: `cat /dir1/file1`
   - **Save and Exit**:
     - Saves the memory content to disk and exits the program. The saved content can be reloaded in future sessions.

---

## Additional Features

In addition to the basic requirements, this project includes the following advanced features:

1. **Secondary Indirect Addressing**:

   - Supports a second level of indirect addressing for larger files.

2. **Hard Links**:
   - Implements hard links, allowing multiple directory entries to reference the same file.

---

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

---

## Contributing

Contributions are welcome! If you’d like to improve this project, feel free to submit a pull request or open an issue. Whether it’s bug fixes, feature suggestions, or documentation improvements, your help is greatly appreciated.

---

## License

This project is licensed under the [MIT License](LICENSE). Feel free to use, modify, and distribute it as per the terms of the license.

---

## Acknowledgments

This project was inspired by the foundational concepts of file systems and aims to provide a practical, hands-on learning experience. Special thanks to everyone who contributed to the development and testing of this project.
