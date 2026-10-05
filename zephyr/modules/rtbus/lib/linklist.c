#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
    void *                  next;
    uint8_t                 __dummy[];
} linklist_t;

/******************************************************************************/

extern void * bmalloc(uint8_t index , size_t n);
extern void bfree( uint8_t index ,void *ptr);

static void * linklist_new(uint8_t pool_id,
                         void ** source,
                         uint16_t new_datalen)
{

    linklist_t *new_tmp = NULL;
    linklist_t *last_tmp;

    if (source == NULL || new_datalen < sizeof(linklist_t)) {
        return NULL;
    }

    last_tmp = (linklist_t*)*source;

    do
    {
        if(last_tmp == NULL || last_tmp->next == NULL)
        {
            break;
        }

        last_tmp = last_tmp->next;
    }
    while(last_tmp != NULL && last_tmp->next != NULL);

    new_tmp = (linklist_t *)bmalloc(pool_id,new_datalen);
    // memset(new_tmp, 0, new_datalen);
    if( new_tmp == 0 )
    {
        return NULL;
    }
    
    if(new_tmp != NULL)
    {
        new_tmp->next           = NULL;
        if(last_tmp == NULL)
        {
            *source = new_tmp;
        }
        else
        {
            last_tmp->next = new_tmp;
        }
    }
    
    return new_tmp;
}

static void linklist_remove(uint8_t pool_id,
                            void ** source,
                            void * kill_target)
{
    linklist_t *cur_tmp = (linklist_t*)*source;
    linklist_t *pre_tmp = NULL;

    while(cur_tmp != NULL)
    {
        if(cur_tmp == kill_target)
        {
            break;
        }

        pre_tmp = cur_tmp;
        cur_tmp = cur_tmp->next;
    }

    if(cur_tmp == NULL)
    {
        return;
    }

    if(pre_tmp != NULL)
    {
        pre_tmp->next = cur_tmp->next;
        cur_tmp->next = NULL;
    }
    else
    {
        if(cur_tmp->next != NULL)
        {
            *source = cur_tmp->next;
        }
        else
        {
            *source = NULL;
        }
    }
    
    bfree(pool_id,cur_tmp);
    return;
}

static void __attribute__((unused)) linklist_reidx(uint8_t pool_id,
                            void ** source,
                            void * reidx_target)
{
    linklist_t *cur_tmp = (linklist_t*)*source;
    linklist_t *pre_tmp = NULL;
    linklist_t *last_tmp  = NULL;

    while(cur_tmp != NULL)
    {
        if(cur_tmp == reidx_target)
        {
            break;
        }

        pre_tmp = cur_tmp;
        cur_tmp = cur_tmp->next;
    }

    if(cur_tmp == NULL)
    {
        return;
    }

    if(pre_tmp != NULL)
    {
        pre_tmp->next = cur_tmp->next;
        cur_tmp->next = NULL;
    }
    else
    {
        if(cur_tmp->next != NULL)
        {
            *source = cur_tmp->next;
            last_tmp = cur_tmp->next;
        }
        else
        {
            last_tmp = NULL;
            *source = NULL;
        }
    }

    do
    {
        if(last_tmp == NULL || last_tmp->next == NULL)
        {
            break;
        }

        last_tmp = last_tmp->next;
    }
    while(last_tmp != NULL && last_tmp->next != NULL);

    if(cur_tmp != NULL)
    {
        if(last_tmp == NULL)
        {
            *source = cur_tmp;
        }
        else
        {
            last_tmp->next = cur_tmp;
        }
        cur_tmp->next           = NULL;
    }
}

static uint32_t __attribute__((unused)) linklist_count( uint8_t pool_id, void ** source)
{
    uint32_t func_count = 0;
    linklist_t *last_tmp = (linklist_t*)*source;

    if(last_tmp != NULL)
    {
        func_count++;
    }
    
    do
    {
        if(last_tmp == NULL || last_tmp->next == NULL)
        {
            break;
        }

        last_tmp = last_tmp->next;
        func_count++;
    }
    while(last_tmp != NULL && last_tmp->next != NULL);
    
    return func_count;
}


static __attribute__((unused)) void * linklist_get(uint8_t pool_id,
                           void ** source,
                           uint32_t index)
{
    uint32_t func_count = 0;
    linklist_t *last_tmp = (linklist_t*)*source;

    if(func_count == index)
    {
        goto FUNC_EXIT;
    }
    
    do
    {
        if(last_tmp == NULL || last_tmp->next == NULL)
        {
            break;
        }

        last_tmp = last_tmp->next;
        func_count++;
        
        if(func_count == index)
        {
            goto FUNC_EXIT;
        }
    }
    while(last_tmp != NULL && last_tmp->next != NULL);
    
    return NULL;
FUNC_EXIT:
    return last_tmp;
}
