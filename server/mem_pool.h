#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__
// https://www.codeproject.com/Articles/27487/Why-to-use-memory-pool-and-how-to-implement-it
class CMemPool
{
private:
    //The purpose of the structure`s definition is that we can operate linkedlist conveniently
    struct _Unit                     //The type of the node of linkedlist.
    {
        struct _Unit *pPrev, *pNext;
    };

    void* m_pMemBlock;                //The address of memory pool.

    //Manage all unit with two linkedlist.
    struct _Unit*    m_pAllocatedMemBlock; //Head pointer to Allocated linkedlist.
    struct _Unit*    m_pFreeMemBlock;      //Head pointer to Free linkedlist.

    unsigned long    m_ulUnitSize; //Memory unit size. There are much unit in memory pool.
    unsigned long    m_ulBlockSize;//Memory pool size. Memory pool is make of memory unit.

public:
    CMemPool(unsigned long lUnitNum = 50, unsigned long lUnitSize = 1024);
    ~CMemPool();
    
    void* Alloc(unsigned long ulSize, bool bUseMemPool = true); //Allocate memory unit
    void Free( void* p );                                   //Free memory unit
};

/*==========================================================
CMemPool:
    Constructor of this class. It allocate memory block from system and create
    a static double linked list to manage all memory unit.

Parameters:
    [in]ulUnitNum
    The number of unit which is a part of memory block.

    [in]ulUnitSize
    The size of unit.
//=========================================================
*/
CMemPool::CMemPool(unsigned long ulUnitNum,unsigned long ulUnitSize) :
    m_pMemBlock(NULL), m_pAllocatedMemBlock(NULL), m_pFreeMemBlock(NULL), 
    m_ulBlockSize(ulUnitNum * (ulUnitSize+sizeof(struct _Unit))), 
    m_ulUnitSize(ulUnitSize)
{    
    m_pMemBlock = malloc(m_ulBlockSize);     //Allocate a memory block.
    
    if(NULL != m_pMemBlock)
    {
        for(unsigned long i=0; i<ulUnitNum; i++)  //Link all mem unit . Create linked list.
        {
            struct _Unit *pCurUnit = (struct _Unit *)( (char *)m_pMemBlock + i*(ulUnitSize+sizeof(struct _Unit)) );
            
            pCurUnit->pPrev = NULL;
            pCurUnit->pNext = m_pFreeMemBlock;    //Insert the new unit at head.
            
            if(NULL != m_pFreeMemBlock)
            {
                m_pFreeMemBlock->pPrev = pCurUnit;
            }
            m_pFreeMemBlock = pCurUnit;
        }
    }    
} 

/*===============================================================
~CMemPool():
    Destructor of this class. Its task is to free memory block.
//===============================================================
*/
CMemPool::~CMemPool()
{
    free(m_pMemBlock);
}

/*================================================================
Alloc:
    To allocate a memory unit. If memory pool can`t provide proper memory unit,
    It will call system function.

Parameters:
    [in]ulSize
    Memory unit size.

    [in]bUseMemPool
    Whether use memory pool.

Return Values:
    Return a pointer to a memory unit.
//=================================================================
*/
void* CMemPool::Alloc(unsigned long ulSize, bool bUseMemPool)
{
    if(    ulSize > m_ulUnitSize || false == bUseMemPool || 
        NULL == m_pMemBlock   || NULL == m_pFreeMemBlock)
    {
        return malloc(ulSize);
    }

    //Now FreeList isn`t empty
    struct _Unit *pCurUnit = m_pFreeMemBlock;
    m_pFreeMemBlock = pCurUnit->pNext;            //Get a unit from free linkedlist.
    if(NULL != m_pFreeMemBlock)
    {
        m_pFreeMemBlock->pPrev = NULL;
    }

    pCurUnit->pNext = m_pAllocatedMemBlock;
    
    if(NULL != m_pAllocatedMemBlock)
    {
        m_pAllocatedMemBlock->pPrev = pCurUnit; 
    }
    m_pAllocatedMemBlock = pCurUnit;

    return (void *)((char *)pCurUnit + sizeof(struct _Unit) );
}

/*================================================================
Free:
    To free a memory unit. If the pointer of parameter point to a memory unit,
    then insert it to "Free linked list". Otherwise, call system function "free".

Parameters:
    [in]p
    It point to a memory unit and prepare to free it.

Return Values:
    none
//================================================================
*/
void CMemPool::Free( void* p )
{
    if(m_pMemBlock<p && p<(void *)((char *)m_pMemBlock + m_ulBlockSize) )
    {
        struct _Unit *pCurUnit = (struct _Unit *)((char *)p - sizeof(struct _Unit) );

        m_pAllocatedMemBlock = pCurUnit->pNext;
        if(NULL != m_pAllocatedMemBlock)
        {
            m_pAllocatedMemBlock->pPrev = NULL;
        }

        pCurUnit->pNext = m_pFreeMemBlock;
        if(NULL != m_pFreeMemBlock)
        {
             m_pFreeMemBlock->pPrev = pCurUnit;
        }

        m_pFreeMemBlock = pCurUnit;
    }
    else
    {
        free(p);
    }
}

#endif //__MEMPOOL_H__
