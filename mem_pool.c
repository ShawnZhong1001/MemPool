#include <stdio.h>
#include <stdlib.h>
#include "mem_pool.h"

// 结构体：内存块
struct mem_block 
{
    unsigned long addr;
    unsigned long size;
    struct mem_block *next;
};

// 结构体：连续的多个内存块构成的内存块组合
struct block_group 
{
    struct mem_block *first_block;
    struct mem_block *last_block;
    struct block_group *next;
};

// 结构体：链表，管理多个内存块组合
struct block_list
{
    struct block_group *first_group;
	struct block_group *last_group;	
};

// 结构体：内存池
struct mem_pool
{
    unsigned int addr_start;
    unsigned int addr_end;
    
    struct block_list used_block_list;
	struct block_list unused_block_list; 
};

// 结构体数组：5种分区的内存池，每种分区使用一个内存池，按PT32/PT16...顺序存放
struct mem_pool memPool[5];
// 存储在每种分区中块的大小，单位为byte
unsigned long blockSize[5] = {32*1024, 16*1024, 8*1024, 4*1024, 2*1024};
// 初始化成功与否标志位
int inited_flag = 0;

// 初始化一个内存池，参数为内存池指针、内存块大小、内存池起始地址、内存块个数
void InitMemPool( struct mem_pool *pool, unsigned long blockSize, unsigned long start_addr, unsigned long blocknum )
{
	/********************初始化pool********************/
    //printf( "InitMemPool, the start_addr is %ld\r\n", start_addr );
    pool->addr_start = start_addr;
    pool->addr_end = start_addr + blockSize * blocknum;  

	struct block_group *group = (struct block_group*)malloc( sizeof(struct block_group) );
	group->first_block = NULL;
	group->last_block = NULL;
	group->next = NULL;

	pool->unused_block_list.first_group = group;
	pool->unused_block_list.last_group = group;

    /********************初始化每个内存块，更新pool********************/
	int i;
	struct mem_block *pBlock;

	for ( i = 0; i < blocknum; ++i )
    {
    	pBlock = (struct mem_block*)malloc( sizeof(struct mem_block) );
		pBlock->addr = pool->addr_start + i * blockSize;
		pBlock->size = blockSize;
		pBlock->next = NULL;

	    if ( pool->unused_block_list.first_group->first_block == NULL )
		{	
			pool->unused_block_list.first_group->first_block = pBlock;
			pool->unused_block_list.first_group->last_block = pBlock;
		}
		else
		{
		    pool->unused_block_list.first_group->last_block->next = pBlock;
		    pool->unused_block_list.first_group->last_block = pBlock;
        }
	}

	inited_flag = 1;	
}

// 从组合链表中移除一个节点
void RemoveGroup( struct block_list *list, struct block_group *group )
{
    struct block_group *preGroup;
	preGroup = list->first_group;
	
    // 第一个内存块组
	if ( preGroup == group )
	{
		list->first_group = preGroup->next;

		return;
	}
	
    // 其它内存块组
	while ( preGroup != NULL )
	{
	    if ( preGroup->next == group )
	    {
		    preGroup->next = group->next;

            // 最后一个内存块组
		    if ( preGroup->next == NULL )
		    {
		        list->last_group = preGroup;	
			}

		    return;
	    } 

		preGroup = preGroup->next;
	}	
}

// 向组合链表中插入一个group，参数combine标志是否将地址相连的两个group合并为一个
// 0表示不合并，用于分配时的used列表；1表示合并，用于释放时的unused列表
void InsertGroup( struct block_list *list, struct block_group *group, int combine )
{
	/********************若list为空********************/
    // 第一个内存块组为空
	if ( list->first_group == NULL )
	{
	    list->first_group = group;
		list->last_group = group;
		group->next = NULL;

		return;	
	}
	
    /********************未到第一个内存组的第一个内存块********************/
	// 第一个内存块组的第一个内存块前端还有足够的空间，则插在list最前面
	if ( group->last_block->addr + group->last_block->size < list->first_group->first_block->addr )
    {
        group->next = list->first_group;
        list->first_group = group;

		return;	
	}
	
	struct block_group *pGroup;
	pGroup = list->first_group;
	
    /********************利用标志进行分场合讨论是否合并********************/
    // combine标志等于0，不合并
	if ( combine == 0 )
	{
		// 插在list中间
		while ( pGroup != NULL && pGroup->next != NULL )
	    {
		    if ( group->last_block->addr + group->last_block->size < pGroup->next->first_block->addr )
		    {
		        group->next = pGroup->next;
			    pGroup->next = group;

			    break;	
		    }	

		    // pGroup = pGroup->next; // ？？这里为什么要注释掉？？->我觉得应该留着，往后移进行更新
		}

		// 如果是最后一个内存块，则直接插在list最后
		if ( pGroup->next == NULL )
		{
		    group->next = NULL;
			pGroup->next = group;	
			list->last_group = group;
		}
	}
    // combine标志等于1，合并
	else
	{
	    while ( pGroup != NULL )
	    {
	    	// 要插入的组和pGroup组的内存块前端相连，合并内存块
			if ( group->last_block->addr + group->last_block->size == pGroup->first_block->addr )
	        {
			    group->last_block->next = pGroup->first_block;
		        pGroup->first_block = group->first_block;
		        free( group );

		        return;	
	        }
	        // 要插入的组和pGroup组的内存块后端相连，合并内存块
	        else if ( pGroup->last_block->addr + pGroup->last_block->size == group->first_block->addr )
	        {
	            pGroup->last_block->next = group->first_block;
	            pGroup->last_block = group->last_block;
			    free( group );

			    // 合并后的新组，判断是否和pGroup->next能合并
			    if ( pGroup->next != NULL && pGroup->last_block->addr + pGroup->last_block->size == pGroup->next->first_block->addr )
		        {
		            pGroup->last_block->next = pGroup->next->first_block;
					pGroup->last_block = pGroup->next->last_block;
					pGroup->next = pGroup->next->next;
					free( pGroup );	
				}

				return;		
		    }
		    // 要插入的组在pGroup组和pGroup->next之间，前后不相连
		    else if ( pGroup->next != NULL && group->last_block->addr + group->last_block->size < pGroup->next->first_block->addr )
		    {
		        group->next = pGroup->next;
			    pGroup->next = group;

			    return;	
		    }
		    // 到链表末端，插入后需要更新list->last_group
		    else if ( pGroup->next == NULL )
		    {
		    	group->next = NULL;
			    pGroup->next = group;	
			    list->last_group = group;

			    return;
			}

		    pGroup = pGroup->next;
	    }
	}	
}

// 接口一：初始化内存管理系统
void MemInit( unsigned long pool_addr, unsigned long block_nums[5] )
{
	int i;
	unsigned long start_addr = pool_addr;

    for ( i = 0; i < 5; ++i )
    {
        InitMemPool( &(memPool[i]), blockSize[i], start_addr, block_nums[i] );
		start_addr += blockSize[i] * block_nums[i];	
	}

}

// 根据poolId，在对应的内存池中分配长度为length字节的内存空间
unsigned long MemAlloc1( int poolId, unsigned long length )
{
    printf( "memAlloc1,poolId is %d\r\n", poolId );
    
    /********************长度换算********************/
    // 换算成分配的块数量
    unsigned long blockNum = length / blockSize[poolId];
    unsigned long t = length % blockSize[poolId]; 

    if ( t > 0 )
    {
         blockNum += 1;
    }

    /********************寻找合适的内存块组********************/
    struct block_group *pGroup = memPool[poolId].unused_block_list.first_group;
	struct block_group *preGroup = NULL;
	
	while ( pGroup != NULL )
	{
	    // 当前内存块组里的内存块足够分配
        // ？？按内存块组来判断，如果前一个内存块组的内存块数量加上下一个的部分可以满足怎么处理呢？？
        if ( ((pGroup->last_block->addr - pGroup->first_block->addr)/blockSize[poolId] + 1) >= blockNum )
	    {
	    	break;
		}

		preGroup = pGroup;
		pGroup = pGroup->next;
    }

    // 没有找到合适的内存块组
    // 先while循环进行查询，后对不符的情况进行处理
    if ( pGroup == NULL )
    {
    	return 0xFFFFFFFF;
	}
	
    /********************进行分配********************/
    struct block_group *newGroup = (struct block_group *)malloc( sizeof(struct block_group) );
	newGroup->first_block = pGroup->first_block;
	newGroup->last_block = pGroup->first_block;
	newGroup->next = NULL;

	int i;
    // 进行blockNum-1次循环，最终last_block指到第blockNum个内存块
	for ( i = 0; i < blockNum-1; ++i )
	{
		newGroup->last_block = newGroup->last_block->next;
	}

    /********************更新两个管理列表********************/
    // 更新first_group的起始地址，更新unused列表
	pGroup->first_block = newGroup->last_block->next;
	newGroup->last_block->next = NULL;

    // 更新used列表
	InsertGroup( &(memPool[poolId].used_block_list), newGroup, 0 );

    // 如果起始地址为空，取下一个来更新unused列表
	if ( pGroup->first_block == NULL )
	{
	    if ( preGroup == NULL )
		{
		    memPool[poolId].unused_block_list.first_group = pGroup->next;
		}
		else
		{
			preGroup->next = pGroup->next;
		}

		free( pGroup ); // 为空，释放掉
	}

	return newGroup->first_block->addr;
}

// 接口二：申请内存
unsigned long MemAlloc( unsigned long length )
{
    /********************未初始化处理********************/
    if ( !inited_flag )
    {
	    return 0xFFFFFFFF;
	}
	
    /********************长度换算********************/
    // 用KB做单位，更符合平时习惯；且分配时需要转换，便将公共运算部分抽取出来放到外面
	unsigned long sizeKB = length / (unsigned long)1024;
	unsigned long t = length % (unsigned long)1024;

    // 入运算
	if ( t > 0 )
	{
        ++sizeKB;     
    }
    
    /********************对长度分情况进行分配********************/
    // 对于超出32KB的申请，系统直接分配失败
    if ( sizeKB > 32 )
    {
    	return 0xFFFFFFFF;
	}
    // 16KB<length<=32KB
	else if ( sizeKB > 16 )
	{
	    // 优先单块，而且只有一种单块
        if ( memPool[0].unused_block_list.first_group != NULL )
		{
		    return MemAlloc1( 0, length );
		}
        // 考虑组合
		else
		{
		    // 利用初始化
            unsigned long addr = 0xFFFFFFFF;
			
            int i = 1;
			while ( addr == 0xFFFFFFFF ) 
			{
			    addr = MemAlloc1( i, length );
				++i;

				if ( i >= 5 )
				{
				    break; // 跳出循环，直接返回地址
				}	
			}
			
			return addr;
		}	
	}
    // 8KB<length<=16KB
	else if ( sizeKB > 8 )
	{
	    if ( memPool[1].unused_block_list.first_group != NULL )
		{
		    return MemAlloc1( 1, length );    	
		}
		else
		{
		    unsigned long addr = 0xFFFFFFFF;
		    // 检查32KB单块
            addr = MemAlloc1( 0, length );
		    if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // 考虑组合
			int i = 2;
			while ( addr == 0xFFFFFFFF ) 
			{
			    addr = MemAlloc1( i, length );
				++i;
				
				if ( i >= 5 )
				{
				    break;
				}	
			}
			
			return addr;
		}	
	}
    // 4KB<length<=8KB
	else if ( sizeKB > 4 )
	{
	    if ( memPool[2].unused_block_list.first_group != NULL )
		{
		    return MemAlloc1( 2, length );    	
		}
		
		else
		{
		    unsigned long addr = 0xFFFFFFFF;
		    // 考虑16KB单块
            addr = MemAlloc1( 1, length );
		    if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // 考虑32KB单块
			addr = MemAlloc1( 0, length );
			if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // 考虑组合
			int i = 3;
			while ( addr == 0xFFFFFFFF ) 
			{
			    addr = MemAlloc1( i, length );
				++i;
				
				if ( i >= 5 )
				{
				    break;
				}	
			}
			
			return addr;
		}	
	}
    // 2KB<length<=4KB
	else if ( sizeKB > 2 )
	{
	    if ( memPool[3].unused_block_list.first_group != NULL )
		{
		    return MemAlloc1( 3, length );    	
		}
		
		else
		{
		    unsigned long addr = 0xFFFFFFFF;
		    // 考虑8KB单块
            addr = MemAlloc1( 2, length );
		    if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // 考虑16KB单块
			addr = MemAlloc1( 1, length );
			if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // 考虑32KB单块
			addr = MemAlloc1( 0, length );
			if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // 考虑组合
			int i = 4;
			while ( addr == 0xFFFFFFFF ) 
			{
			    addr = MemAlloc1( i, length );
				++i;
				if ( i >= 5 )
				{
				    break;
				}	
			}
			
			return addr;
		}	
	}
    // length<=2KB
	else
	{
	    if ( memPool[4].unused_block_list.first_group != NULL )
		{
		    return MemAlloc1( 4, length );    	
		}
		else
		{
		    unsigned long addr = 0xFFFFFFFF;
			int i = 3; 
			
            // 只有单块，没有组合
			while ( addr == 0xFFFFFFFF ) 
			{
			    addr = MemAlloc1( i, length );
				--i;
				if ( i < 0 )
				{
				    break;
				}	
			}
			
			return addr;
		}	
	}
	
	return 0xFFFFFFFF;
}

// 接口三：释放内存
unsigned long MemFree( unsigned long addr )
{
	/********************未初始化处理********************/
    // 内存管理系统没有初始化
    if ( !inited_flag )
    {
	    return 1;
	}

    /********************确定释放地址在哪个内存池中********************/
	int i;
    for ( i = 0; i < 5; ++i )
	{
	    if ( addr >= memPool[i].addr_start && addr < memPool[i].addr_end )
	    {
	    	break; // 但凡进入了这里面，就说明地址在内存管理系统里，i就不会等于5
		}
	}
    // addr不是本内存管理系统分配的内存首地址
	if ( i == 5 )
	{
	    return 1;
	}	

    /********************以内存块组为单位进行释放********************/
	struct block_group *pGroup = memPool[i].used_block_list.first_group;

	while ( pGroup != NULL )
	{
		if ( pGroup->first_block->addr == addr )
		{
			RemoveGroup( &(memPool[i].used_block_list), pGroup );
			InsertGroup( &(memPool[i].unused_block_list), pGroup, 1 );
			
			return 0;
		}

		pGroup = pGroup->next;
	}

	return 1;
}

// 接口四：清空所有数据
void Clear()
{
	int i;
	struct block_group *pGroup, *tempGroup;
	struct mem_block *pBlock, *tempBlock;

    // 按内存池进行释放
    for ( i = 0; i < 5; ++i )
	{
	    // 释放已使用内存块列表
        pGroup = memPool[i].used_block_list.first_group;
	    while ( pGroup != NULL )
		{
			tempGroup = pGroup->next;
			pBlock = pGroup->first_block;
            
			while ( pBlock != NULL )
			{
				tempBlock = pBlock->next;
				free( pBlock );
				
                pBlock = tempBlock;
			}
			free( pGroup );
			
			pGroup = tempGroup;
		}
		
        // 释放未使用内存块列表
		pGroup = memPool[i].unused_block_list.first_group;
	    while ( pGroup != NULL )
		{
			tempGroup = pGroup->next;
			pBlock = pGroup->first_block;
			while ( pBlock != NULL )
			{
				tempBlock = pBlock->next;
				free( pBlock );
                
				pBlock = tempBlock;
			}
			free( pGroup );
			
			pGroup = tempGroup;
		}
		
		memPool[i].unused_block_list.first_group = NULL;
		memPool[i].unused_block_list.last_group = NULL;	
	}
}


