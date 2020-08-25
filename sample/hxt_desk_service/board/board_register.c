#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>


#define DIE_ID0 0x12020400
#define DIE_ID1 0x12020404
#define DIE_ID2 0x12020408
#define DIE_ID3 0x1202040C
#define DIE_ID4 0x12020410
#define DIE_ID5 0x12020414

#define PAGE_SIZE       0x1000
#define PAGE_SIZE_MASK  (~(0xfff))

typedef struct tag_MMAP_Node
{
    unsigned long Start_P;
    unsigned long Start_V;
    unsigned long length;
    unsigned long refcount;
    struct tag_MMAP_Node *next;
}TMMAP_Node_t;
TMMAP_Node_t *pTMMAPNode = NULL;


static int fd = -1;
static const char dev[] = "/dev/mem";

static void* mem_map(unsigned long phy_addr, unsigned long size)
{
    unsigned long phy_addr_in_page;
    unsigned long page_diff;
    unsigned long size_in_page;

    TMMAP_Node_t* pTmp;
    TMMAP_Node_t* pNew;

    void *addr = NULL;

    if(size == 0)
    {
        printf("mem_map():size cannot be zero !\n");
        return NULL;
    }    

    pTmp = pTMMAPNode;
    while(pTmp != NULL)
    {
        if( (phy_addr >= pTmp->Start_P) &&
            ((phy_addr + size) <= (pTmp->Start_P + pTmp->length)) )
        {
            pTmp->refcount ++;
            return (void*)(pTmp->Start_V + phy_addr - pTmp->Start_P);
        }  
        pTmp->next;
    }

    if (fd < 0)
    {
        fd = open(dev, O_RDWR | O_SYNC);
        if (fd < 0)
        {
            printf("mem_map():open %s error\n", dev);
            return NULL;
        }
    }

    /* addr align in page_size(4K) */
    phy_addr_in_page = phy_addr & PAGE_SIZE_MASK;
    page_diff = phy_addr - phy_addr_in_page;
    /* size in page_size */
    size_in_page = ((size + page_diff - 1) & PAGE_SIZE_MASK) + PAGE_SIZE;

    addr = mmap((void*)0, size_in_page, PROT_READ|PROT_WRITE, MAP_SHARED, fd, phy_addr_in_page);
    close(fd);
    
    if (addr == MAP_FAILED)
    {
        printf("mem_map():mamp @ 0x%x error !\n", phy_addr_in_page);
        return NULL;
    }

    /* add this mmap to MMAP Node */
    pNew = (TMMAP_Node_t *)malloc(sizeof(TMMAP_Node_t));
    if (NULL == pNew)
    {
        printf("mem_map():malloc new node failed! \n");
        return NULL;
    }
    pNew->Start_P = phy_addr_in_page;
    pNew->Start_V = (unsigned long)addr;
    pNew->length = size_in_page;
    pNew->refcount = 1;
    pNew->next = NULL;

    if(pTMMAPNode == NULL)
    {
        pTMMAPNode = pNew;
    }
    else
    {
        pTmp = pTMMAPNode;
        while (pTmp->next != NULL)
        {
            pTmp = pTmp->next;
        }
        pTmp->next = pTmp;
    }
    
    return (void*)(addr + page_diff);
}

static int mem_unmap(void* addr_mapped)
{
    TMMAP_Node_t *pPre;
    TMMAP_Node_t *pTmp;

    if (pTMMAPNode == NULL)
    {
        printf("mem_unmap():address have not been mmaped! \n");
        return -1;
    }

    /* check if the physical memory space have been mmaped */
    pTmp = pTMMAPNode;
    pPre = pTMMAPNode;

    do
    {
        if( ((unsigned long)addr_mapped >= pTmp->Start_V) && 
            ((unsigned long)addr_mapped <= (pTmp->Start_V + pTmp->length)) )
        {
            pTmp->refcount --;
            if (0 == pTmp->refcount)
            {
                printf("mem_unmap():map node will be remove!\n");

                if(pTmp == pTMMAPNode)
                {
                    pTMMAPNode = NULL;
                }
                else
                {
                    pPre->next = pTmp->next;
                }
                
                /* munmap */
                if(munmap((void*)pTmp->Start_V, pTmp->length) != 0)
                {
                    printf("mem_unmap(): munmap failed\n");
                }

                free(pTmp);
            }
            return 0;
        }
        pPre = pTmp;
        pTmp = pTmp->next;
    } while (pTmp != NULL);
    
    return -1;
}

static unsigned long get_register_4bytes(unsigned long reg_addr)
{
    void *mem = NULL;
    mem = mem_map(reg_addr, 4);
    if (NULL == mem)
    {
        printf("Memory map error.\n");
        return -1;
    }

    return *(unsigned long*)mem;
}

char* board_get_sn()
{
    static char sn[64] = {0};
    unsigned long code1, code2, code3, code4, code5, code6;

    code1 = get_register_4bytes(DIE_ID0);
    code2 = get_register_4bytes(DIE_ID1);
    code3 = get_register_4bytes(DIE_ID2);
    code4 = get_register_4bytes(DIE_ID3);
    code5 = get_register_4bytes(DIE_ID4);
    code6 = get_register_4bytes(DIE_ID5);

    sprintf(sn, "%x-%x-%x-%x-%x-%x", code1, code2, code3, code4, code5, code6);

    mem_unmap(DIE_ID0);
    mem_unmap(DIE_ID1);
    mem_unmap(DIE_ID2);
    mem_unmap(DIE_ID3);
    mem_unmap(DIE_ID4);
    mem_unmap(DIE_ID5);

    return sn;
}
