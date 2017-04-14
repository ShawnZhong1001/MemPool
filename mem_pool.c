#include <stdio.h>
#include <stdlib.h>
#include "mem_pool.h"

// �ṹ�壺�ڴ��
struct mem_block 
{
    unsigned long addr;
    unsigned long size;
    struct mem_block *next;
};

// �ṹ�壺�����Ķ���ڴ�鹹�ɵ��ڴ�����
struct block_group 
{
    struct mem_block *first_block;
    struct mem_block *last_block;
    struct block_group *next;
};

// �ṹ�壺�����������ڴ�����
struct block_list
{
    struct block_group *first_group;
	struct block_group *last_group;	
};

// �ṹ�壺�ڴ��
struct mem_pool
{
    unsigned int addr_start;
    unsigned int addr_end;
    
    struct block_list used_block_list;
	struct block_list unused_block_list; 
};

// �ṹ�����飺5�ַ������ڴ�أ�ÿ�ַ���ʹ��һ���ڴ�أ���PT32/PT16...˳����
struct mem_pool memPool[5];
// �洢��ÿ�ַ����п�Ĵ�С����λΪbyte
unsigned long blockSize[5] = {32*1024, 16*1024, 8*1024, 4*1024, 2*1024};
// ��ʼ���ɹ�����־λ
int inited_flag = 0;

// ��ʼ��һ���ڴ�أ�����Ϊ�ڴ��ָ�롢�ڴ���С���ڴ����ʼ��ַ���ڴ�����
void InitMemPool( struct mem_pool *pool, unsigned long blockSize, unsigned long start_addr, unsigned long blocknum )
{
	/********************��ʼ��pool********************/
    //printf( "InitMemPool, the start_addr is %ld\r\n", start_addr );
    pool->addr_start = start_addr;
    pool->addr_end = start_addr + blockSize * blocknum;  

	struct block_group *group = (struct block_group*)malloc( sizeof(struct block_group) );
	group->first_block = NULL;
	group->last_block = NULL;
	group->next = NULL;

	pool->unused_block_list.first_group = group;
	pool->unused_block_list.last_group = group;

    /********************��ʼ��ÿ���ڴ�飬����pool********************/
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

// ������������Ƴ�һ���ڵ�
void RemoveGroup( struct block_list *list, struct block_group *group )
{
    struct block_group *preGroup;
	preGroup = list->first_group;
	
    // ��һ���ڴ����
	if ( preGroup == group )
	{
		list->first_group = preGroup->next;

		return;
	}
	
    // �����ڴ����
	while ( preGroup != NULL )
	{
	    if ( preGroup->next == group )
	    {
		    preGroup->next = group->next;

            // ���һ���ڴ����
		    if ( preGroup->next == NULL )
		    {
		        list->last_group = preGroup;	
			}

		    return;
	    } 

		preGroup = preGroup->next;
	}	
}

// ����������в���һ��group������combine��־�Ƿ񽫵�ַ����������group�ϲ�Ϊһ��
// 0��ʾ���ϲ������ڷ���ʱ��used�б�1��ʾ�ϲ��������ͷ�ʱ��unused�б�
void InsertGroup( struct block_list *list, struct block_group *group, int combine )
{
	/********************��listΪ��********************/
    // ��һ���ڴ����Ϊ��
	if ( list->first_group == NULL )
	{
	    list->first_group = group;
		list->last_group = group;
		group->next = NULL;

		return;	
	}
	
    /********************δ����һ���ڴ���ĵ�һ���ڴ��********************/
	// ��һ���ڴ����ĵ�һ���ڴ��ǰ�˻����㹻�Ŀռ䣬�����list��ǰ��
	if ( group->last_block->addr + group->last_block->size < list->first_group->first_block->addr )
    {
        group->next = list->first_group;
        list->first_group = group;

		return;	
	}
	
	struct block_group *pGroup;
	pGroup = list->first_group;
	
    /********************���ñ�־���зֳ��������Ƿ�ϲ�********************/
    // combine��־����0�����ϲ�
	if ( combine == 0 )
	{
		// ����list�м�
		while ( pGroup != NULL && pGroup->next != NULL )
	    {
		    if ( group->last_block->addr + group->last_block->size < pGroup->next->first_block->addr )
		    {
		        group->next = pGroup->next;
			    pGroup->next = group;

			    break;	
		    }	

		    // pGroup = pGroup->next; // ��������ΪʲôҪע�͵�����->�Ҿ���Ӧ�����ţ������ƽ��и���
		}

		// ��������һ���ڴ�飬��ֱ�Ӳ���list���
		if ( pGroup->next == NULL )
		{
		    group->next = NULL;
			pGroup->next = group;	
			list->last_group = group;
		}
	}
    // combine��־����1���ϲ�
	else
	{
	    while ( pGroup != NULL )
	    {
	    	// Ҫ��������pGroup����ڴ��ǰ���������ϲ��ڴ��
			if ( group->last_block->addr + group->last_block->size == pGroup->first_block->addr )
	        {
			    group->last_block->next = pGroup->first_block;
		        pGroup->first_block = group->first_block;
		        free( group );

		        return;	
	        }
	        // Ҫ��������pGroup����ڴ�����������ϲ��ڴ��
	        else if ( pGroup->last_block->addr + pGroup->last_block->size == group->first_block->addr )
	        {
	            pGroup->last_block->next = group->first_block;
	            pGroup->last_block = group->last_block;
			    free( group );

			    // �ϲ�������飬�ж��Ƿ��pGroup->next�ܺϲ�
			    if ( pGroup->next != NULL && pGroup->last_block->addr + pGroup->last_block->size == pGroup->next->first_block->addr )
		        {
		            pGroup->last_block->next = pGroup->next->first_block;
					pGroup->last_block = pGroup->next->last_block;
					pGroup->next = pGroup->next->next;
					free( pGroup );	
				}

				return;		
		    }
		    // Ҫ���������pGroup���pGroup->next֮�䣬ǰ������
		    else if ( pGroup->next != NULL && group->last_block->addr + group->last_block->size < pGroup->next->first_block->addr )
		    {
		        group->next = pGroup->next;
			    pGroup->next = group;

			    return;	
		    }
		    // ������ĩ�ˣ��������Ҫ����list->last_group
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

// �ӿ�һ����ʼ���ڴ����ϵͳ
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

// ����poolId���ڶ�Ӧ���ڴ���з��䳤��Ϊlength�ֽڵ��ڴ�ռ�
unsigned long MemAlloc1( int poolId, unsigned long length )
{
    printf( "memAlloc1,poolId is %d\r\n", poolId );
    
    /********************���Ȼ���********************/
    // ����ɷ���Ŀ�����
    unsigned long blockNum = length / blockSize[poolId];
    unsigned long t = length % blockSize[poolId]; 

    if ( t > 0 )
    {
         blockNum += 1;
    }

    /********************Ѱ�Һ��ʵ��ڴ����********************/
    struct block_group *pGroup = memPool[poolId].unused_block_list.first_group;
	struct block_group *preGroup = NULL;
	
	while ( pGroup != NULL )
	{
	    // ��ǰ�ڴ��������ڴ���㹻����
        // �������ڴ�������жϣ����ǰһ���ڴ������ڴ������������һ���Ĳ��ֿ���������ô�����أ���
        if ( ((pGroup->last_block->addr - pGroup->first_block->addr)/blockSize[poolId] + 1) >= blockNum )
	    {
	    	break;
		}

		preGroup = pGroup;
		pGroup = pGroup->next;
    }

    // û���ҵ����ʵ��ڴ����
    // ��whileѭ�����в�ѯ����Բ�����������д���
    if ( pGroup == NULL )
    {
    	return 0xFFFFFFFF;
	}
	
    /********************���з���********************/
    struct block_group *newGroup = (struct block_group *)malloc( sizeof(struct block_group) );
	newGroup->first_block = pGroup->first_block;
	newGroup->last_block = pGroup->first_block;
	newGroup->next = NULL;

	int i;
    // ����blockNum-1��ѭ��������last_blockָ����blockNum���ڴ��
	for ( i = 0; i < blockNum-1; ++i )
	{
		newGroup->last_block = newGroup->last_block->next;
	}

    /********************�������������б�********************/
    // ����first_group����ʼ��ַ������unused�б�
	pGroup->first_block = newGroup->last_block->next;
	newGroup->last_block->next = NULL;

    // ����used�б�
	InsertGroup( &(memPool[poolId].used_block_list), newGroup, 0 );

    // �����ʼ��ַΪ�գ�ȡ��һ��������unused�б�
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

		free( pGroup ); // Ϊ�գ��ͷŵ�
	}

	return newGroup->first_block->addr;
}

// �ӿڶ��������ڴ�
unsigned long MemAlloc( unsigned long length )
{
    /********************δ��ʼ������********************/
    if ( !inited_flag )
    {
	    return 0xFFFFFFFF;
	}
	
    /********************���Ȼ���********************/
    // ��KB����λ��������ƽʱϰ�ߣ��ҷ���ʱ��Ҫת�����㽫�������㲿�ֳ�ȡ�����ŵ�����
	unsigned long sizeKB = length / (unsigned long)1024;
	unsigned long t = length % (unsigned long)1024;

    // ������
	if ( t > 0 )
	{
        ++sizeKB;     
    }
    
    /********************�Գ��ȷ�������з���********************/
    // ���ڳ���32KB�����룬ϵͳֱ�ӷ���ʧ��
    if ( sizeKB > 32 )
    {
    	return 0xFFFFFFFF;
	}
    // 16KB<length<=32KB
	else if ( sizeKB > 16 )
	{
	    // ���ȵ��飬����ֻ��һ�ֵ���
        if ( memPool[0].unused_block_list.first_group != NULL )
		{
		    return MemAlloc1( 0, length );
		}
        // �������
		else
		{
		    // ���ó�ʼ��
            unsigned long addr = 0xFFFFFFFF;
			
            int i = 1;
			while ( addr == 0xFFFFFFFF ) 
			{
			    addr = MemAlloc1( i, length );
				++i;

				if ( i >= 5 )
				{
				    break; // ����ѭ����ֱ�ӷ��ص�ַ
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
		    // ���32KB����
            addr = MemAlloc1( 0, length );
		    if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // �������
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
		    // ����16KB����
            addr = MemAlloc1( 1, length );
		    if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // ����32KB����
			addr = MemAlloc1( 0, length );
			if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // �������
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
		    // ����8KB����
            addr = MemAlloc1( 2, length );
		    if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // ����16KB����
			addr = MemAlloc1( 1, length );
			if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // ����32KB����
			addr = MemAlloc1( 0, length );
			if ( addr != 0xFFFFFFFF )
		    {
		    	return addr;
			}
			
            // �������
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
			
            // ֻ�е��飬û�����
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

// �ӿ������ͷ��ڴ�
unsigned long MemFree( unsigned long addr )
{
	/********************δ��ʼ������********************/
    // �ڴ����ϵͳû�г�ʼ��
    if ( !inited_flag )
    {
	    return 1;
	}

    /********************ȷ���ͷŵ�ַ���ĸ��ڴ����********************/
	int i;
    for ( i = 0; i < 5; ++i )
	{
	    if ( addr >= memPool[i].addr_start && addr < memPool[i].addr_end )
	    {
	    	break; // ���������������棬��˵����ַ���ڴ����ϵͳ�i�Ͳ������5
		}
	}
    // addr���Ǳ��ڴ����ϵͳ������ڴ��׵�ַ
	if ( i == 5 )
	{
	    return 1;
	}	

    /********************���ڴ����Ϊ��λ�����ͷ�********************/
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

// �ӿ��ģ������������
void Clear()
{
	int i;
	struct block_group *pGroup, *tempGroup;
	struct mem_block *pBlock, *tempBlock;

    // ���ڴ�ؽ����ͷ�
    for ( i = 0; i < 5; ++i )
	{
	    // �ͷ���ʹ���ڴ���б�
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
		
        // �ͷ�δʹ���ڴ���б�
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


