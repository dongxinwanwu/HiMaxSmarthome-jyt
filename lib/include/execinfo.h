//
// Created by wjian on 2022/7/20.
//

#ifndef __BACKTRACE_H__
#define __BACKTRACE_H__

int backtrace(void **buffer, int size);
char **backtrace_symbols(void *const *buffer, int size);
void backtrace_symbols_fd(void *const *buffer, int size, int fd);

#endif
