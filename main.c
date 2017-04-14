#include <stdio.h>
#include <stdlib.h>
#include "mem_pool.h"

int main(int argc, char *argv[])
{
    unsigned long block_nums[5] = {1,2,4,8,16};
    unsigned long memAddr = 0;
    
    MemInit( memAddr, block_nums );
    
    unsigned long addr = MemAlloc( 32*1024 );
    printf( "MemAlloc( 32 ), the addr is %ld\r\n", addr );
    
    addr = MemAlloc( 32*1024 );
    printf( "MemAlloc( 32 ), the addr is %ld\r\n", addr );
    
    MemFree( memAddr );
    Clear();
    
    return 0;
}
