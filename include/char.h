#ifndef CHAR_H
#define CHAR_H

#include <sea/dm/char.h>

void init_char_devs();
chardevice_t *set_chardevice(int maj, int (*f)(int, int, char*, size_t), 
	int (*c)(int, int, long), int (*s)(int, int));
int char_ioctl(dev_t dev, int cmd, long arg);
int set_availablecd(int (*f)(int, int, char*, size_t), int (*c)(int, int, long), 
	int (*s)(int, int));
void unregister_char_device(int n);
int ttyx_rw(int rw, int min, char *buf, size_t count);
int tty_rw(int rw, int min, char *buf, size_t count);
int tty_select(int, int);
int ttyx_select(int, int);
#endif
