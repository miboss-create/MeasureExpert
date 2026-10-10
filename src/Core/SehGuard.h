#ifndef SEHGUARD_H
#define SEHGUARD_H

/** 安装未处理异常过滤器，把模块+偏移写到 runtime/log/crash.txt */
void installCrashDump();

#endif // SEHGUARD_H
