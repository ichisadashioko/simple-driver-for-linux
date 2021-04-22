# Linux Device Drivers: Tutorial for Linux Driver Development

Programming a device driver for Linux requires a deep understanding of the operating system and strong development skills. To help you master this complex domain, Apriorit driver development experts created this tutorial.

We'll show you how to write a device driver for Linux (5.3.0 version of the kernel). In doing so, we'll discuss the kernel logging system, principles of working with kernel modules, character devices, the `file_operations` structure, and accessing user-level memory from the kernel. You'll also get code for a simple Linux driver that you can augment with any functionality you need.

This article will be useful for developers studying Linux driver development.

# Getting started with the Linux kernel module

The Linux kernel is written in the C and Assembler programming languages. C implements the main part of the kernel, while Assembler implements architecture-dependent parts. That's why we can use only these two languages for Linux device driver development. We cannot use C++, which is used for the Microsoft Windows kernel, because some parts of the Linux kernel source code (e.g. header files) may include keywords from C++ (for example, `delete` or `new`), while in Assembler we may encounter lexemes such as ` : : `.

There are two ways of programming a Linux device driver:

1. Compile the driver along with the kernel, which is monolithic in Linux.
2. Implement the driver as a kernel module, in which case you won't need to recompile the kernel.

In this tutorial, we'll develop a driver in form of a kernel module. A module is a specifically designed object file. When working with modules, Linux links them to the kernel by loading them to the kernel address space.

Module code has to operate in the kernel context. This requires a developer to be very attentive. If a developer makes a mistake when implementing a user-level application, it will not cause problems outside the user application in most cases. But mistakes in the implementation of a kernel module will lead to system-level issues.

Luckily for us, the Linux kernel is resistant to non-critical errors in module code. When the kernel encounters such errors (for example, null pointer dereferencing), it displays the `oops` message - an indicator of insignificant malfunctions during Linux operation. After that, the malfunctioning module is unloaded, allowing the kernel and other modules to work as usual. In addition, you can analyze logs that precisely describe non-critical errors. Keep in mind that continuing driver execution after an `oops` message may lead to instability and kernel panic.

The kernel and its modules represent a single program module and use a single global namespace. In order to minimize the namespace, you must control what's exported by the module. Exported global characters must have unique names and be cut to the bare minimum. A commonly used workaround is to simply use the name of the module that's exporting the characters as the prefix for a global character name.

With this basic information in mind, let's start writing our driver for Linux.

# Creating a kernel module

We'll start by creating a simple prototype of a kernel module that can be loaded and unloaded. We can do that with the following code:

```c
#include <linux/init.h>
#include <linux/module.h>

static int my_init(void)
{
    return 0;
}

static void my_exit(void)
{
    return;
}

module_init(my_init);
module_exit(my_exit);
```

The `my_init` function is the driver initialization entry point and is called during system startup (if the driver is statically compiled into the kernel) or when the module is inserted into the kernel. The `my_exit` function is the driver exit point. It's called when unloading a module from the Linux kernel. This function has no effect if the driver is statically compiled into the kernel.

These functions are declared in the `linux/module.h` header file. The `my_init` and `my_exit` functions must have identical signatures such as these:

```c
int init(void);
void exit(void);

Now our simple module is complete. Let's teach it to log in to the kernel and interact with device files. These operations will be useful for Linux kernel driver development.

# Registering a character device

Device files are usually stored in the `/dev` folder. They facilitate interactions between the user and the kernel code. To make the kernel receive anything, you can just write it to a device file to pass it to the module serving this file. Anything that's read from a device file originates from the module serving it.

There are __two groups of device files__:

1. _Character files_ - Non-buffered files that allow you to read and write data character by character. We'll focus on this type of file in this tutorial.
2. Block files - Buffered files that allow you to read and write only whole blocks of data.

Linux systems have __two ways of identifying device files__:

1. _Major device numbers_ identify modules serving device files or groups of devices.
2. _Minor device numbers_ identify specific devices among a group of devices specified by a major device number.

We can define these numbers in the driver code, or they can be allocated dynamically. In case a number defined as a constant has already been used, the system will return an error. When a number is allocated dynamically, the function reserves that number to prevent other device files from using the same number.

To register a character device, we need to use the `register_chrdev` function:

```c
int register_chrdev (
    unsigned int major,
    const char * name,
    const struct file_operations * fops
);
```

Here, we specify the name and major number of a device to register it. After that, the device and the `file_operations` structure will be linked. If we assign 0 to the major parameter, the function will allocate a major device number on its own. If the value returned is 0, this indicates success, while a negative number indicates an error. Both device numbers are specified in the 0-255 range.

> The `file_operations` structure contains pointers to functions defined by the driver. Each field of the structure corresponds to the address of a function defined by the driver to handle a requested operations.

The device name is a string value of the `name` parameter. This string can pass the name of a module if it registers a single device. We use this string to identify a device in the `/sys/devices` file. Device file operations such as `read`, `write`, and `save` are processed by the function pointers stored within the `file_operations` structure. These functions are implemented by the module, and the pointer to the `module` structure identifying this module is also stored within the `file_operations` structure (more about this structure in the next section).

# The `file_operations` structure

In the Linux 5.3.0 kernel, the `file_operations` structure looks like this:

```c
struct file_operations {
  struct module *owner;
  loff_t (*llseek) (struct file *, loff_t, int);
  ssize_t (*read) (struct file *, const char __user *, size_t, loff_t *);
  ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
  ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
  ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
  int (*iopoll) (struct kiocb *kiocb, bool spin);
  int (*iterate) (struct file *, struct dir_context *);
  int (*iterate_shared) (struct file *, struct dir_context *);
  __poll_t (*poll) (struct file *, struct poll_table_struct *);
  long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
  long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
  int (*nmap) (struct file *, struct vm_area_struct *);
  unsigned long nmap_supported_flags;
  int (*open) (struct inode *, struct file *);
  int (*flush) (struct file *, fl_owner_t id);
