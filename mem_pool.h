#ifndef MEM_POOL_H
#define MEM_POOL_H

// 接口一：初始化内存管理系统
void MemInit( unsigned long pool_addr, unsigned long block_nums[5] );

// 接口二：申请内存
unsigned long MemAlloc( unsigned long length );

// 接口三：释放内存
unsigned long MemFree( unsigned long addr );

// 接口四：清空所有数据
void Clear();

#endif