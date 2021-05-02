#ifndef DEVICE_FILE_H_
#define DEVICE_FILE_H_
#include <linux/compiler.h> /* __must_check_ */

__must_check int register_device(void); /* 0 if OK */

void unregister_device(void);

#endif // DEVICE_FILE_H_
