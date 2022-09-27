#include "utils.h"

void processError(size_t error)
{
    if (!error)
        fprintf(stderr,
                "No errors.\n");
    if (error & CANT_ALLOCATE_MEMORY_FOR_STACK)
        fprintf(stderr,
                "Can't allocate memory for stack.\n");
    if (error & CANT_ALLOCATE_MEMORY)
        fprintf(stderr,
                "Can't allocate memory.\n");
    if (error & STACK_IS_EMPTY)
        fprintf(stderr,
                "Can't pop element from stack. Stack is empty.\n");
    if (error & SIZE_MORE_THAN_CAPACITY)
        fprintf(stderr,
                "Size more than capacity.\n");
    if (error & POISON_PTR_ERR)
        fprintf(stderr,
                "Trying to write to poisoned pointer.\n");

    if (error & POISONED_SIZE_ERR)
        fprintf(stderr,
                "Get poisoned stack size.\n");

    if (error & POISONED_CAPACITY_ERR)
        fprintf(stderr,
                "Get poisoned stack capacity.\n");

}

void *recalloc(void *memory,
               size_t currentSize,
               size_t newSize,
               size_t *error)
{
    assert(memory != nullptr);

    if (error != nullptr and *error)
        return error;

    void *newMemory = realloc(memory, newSize);
    if (newMemory == nullptr)
    {
        if (error != nullptr)
            *error = CANT_ALLOCATE_MEMORY;
        return memory;
    }

    if (currentSize < newSize)
        memset((char *) newMemory + currentSize,
               0,
               newSize - currentSize);

    if (error != nullptr)
        *error = NO_ERRORS;

    return newMemory;
}

size_t stackVerifier(Stack *stack)
{
    assert(stack != nullptr);

    size_t error = NO_ERRORS;
    if (stack->size == (size_t) POISON_INT_VALUE)
    {
        error |= POISONED_SIZE_ERR;
    }

    if (stack->capacity == (size_t) POISON_INT_VALUE)
    {
        error |= POISONED_CAPACITY_ERR;
    }

    if (stack->size > stack->capacity)
    {
        error |= SIZE_MORE_THAN_CAPACITY;
    }

    if (stack->data == POISON_PTR or stack->data == nullptr)
    {
        error |= POISON_PTR_ERR;
    }

# if (HashProtection)
    if (stack->hash != stackHash(stack))
    {
        error |= INCORRECT_HASH;
    }
# endif

    if (!stack->alive)
    {
        error |= NOT_ALIVE;
    }

# if (CanaryProtection)
    if (stack->canary_start != CANARY_START)
    {
        if (stack->canary_start == CANARY_POISONED)
        {
            error |= START_STRUCT_CANARY_POISONED;
        }
        else
            error |= START_STRUCT_CANARY_DEAD;
    }

    if (stack->canary_end != CANARY_END)
    {
        if (stack->canary_end == CANARY_POISONED)
        {
            error |= START_STRUCT_CANARY_POISONED;
        }
        else
            error |= START_STRUCT_CANARY_DEAD;
    }

    if (stack->info.canary_start != CANARY_START)
    {
        if (stack->info.canary_start == CANARY_POISONED)
        {
            error |= START_STRUCT_CANARY_POISONED;
        }
        else
            error |= START_STRUCT_CANARY_DEAD;
    }

    if (stack->info.canary_end != CANARY_END)
    {
        if (stack->info.canary_end == CANARY_POISONED)
        {
            error |= START_STRUCT_CANARY_POISONED;
        }
        else
            error |= START_STRUCT_CANARY_DEAD;
    }
# endif

    return error;
}

size_t __stackCtor(Stack *stack, size_t numOfElements)
{
    assert(stack != nullptr);

    size_t error = NO_ERRORS;

    stack->data =
        (Elem_t *) calloc(numOfElements + 1, sizeof(stack->data[0]));
    if (stack->data == nullptr)
        return CANT_ALLOCATE_MEMORY_FOR_STACK;
    stack->size = 0;
    stack->capacity = numOfElements;

    for (size_t i = 0; i < stack->capacity; i++)
    {
        stack->data[i] = POISON_VALUE;
    }
    stack->alive = true;

# if (CanaryProtection)
    stack->info.canary_start = CANARY_START;
    stack->info.canary_end = CANARY_END;

    stack->canary_start = CANARY_START;
    stack->canary_end = CANARY_END;
# endif
# if (HashProtection)
    stack->hash = stackHash(stack);
#endif
    ASSERT_OK(stack, &error)

    return error;
}

size_t stackPush(Stack *stack, Elem_t value)
{
    assert(stack != nullptr);

    size_t error = NO_ERRORS;

    ASSERT_OK(stack, &error)
    if (error)
        return error;

    if (stack->size == stack->capacity)
        error = stackResize(stack);

    if (error)
        return error;

    stack->data[stack->size++] = value;
# if (HashProtection)
    stack->hash = stackHash(stack);
# endif
    ASSERT_OK(stack, &error)

    return error;
}

size_t stackPop(Stack *stack, Elem_t *value)
{
    assert(stack != nullptr);
    assert(value != nullptr);

    size_t error = NO_ERRORS;
    ASSERT_OK(stack, &error)
    if (error)
        return error;

    if (stack->size == 0)
    {
        *value = POISON_VALUE;
        return STACK_IS_EMPTY;
    }

    *value = stack->data[stack->size--];

# if (HashProtection)
    stack->hash = stackHash(stack);
# endif
    if (stack->size * 4 <= stack->capacity)
        error = stackResize(stack);

# if (HashProtection)
    stack->hash = stackHash(stack);
# endif
    ASSERT_OK(stack, &error)

    return error;
}

size_t stackDtor(Stack *stack)
{
    assert(stack != nullptr);

    size_t error = NO_ERRORS;

    ASSERT_OK(stack, &error)

    if (error)
        return error;

    free(stack->data);

    stack->data = (Elem_t *) POISON_PTR;
    stack->size = (size_t) POISON_INT_VALUE;
    stack->capacity = (size_t) POISON_INT_VALUE;

    stack->alive = false;

# if (CanaryProtection)
    stack->info.canary_start = CANARY_POISONED;
    stack->info.canary_end = CANARY_POISONED;

    stack->canary_start = CANARY_POISONED;
    stack->canary_end = CANARY_POISONED;
#endif

# if (HashProtection)
    stack->hash = (size_t) POISON_INT_VALUE;
# endif
    return error;
}

void stackDump(Stack *stack, StackInfo *info)
{
    assert(stack != nullptr);

    printf("Error in function '%s' at %s (%d)\n",
           info->initFunction,
           info->initFile,
           info->initLine);

    printf("Stack [%p] was initialized at %s at %s (%d)\n",
           stack,
           stack->info.initFunction,
           stack->info.initFile,
           stack->info.initLine);
# if (HashProtection)
    printf("{\n"
           "    Size = %zu \n"
           "    Capacity = %zu \n"
           "    Hash = %zu \n"
           "    Correct Hash = %zu \n"
           "    Data [%p] \n",
           stack->size,
           stack->capacity,
           stackHash(stack),
           stack->hash,
           stack->data);
#else
    printf("{\n"
           "    Size = %zu \n"
           "    Capacity = %zu \n"
           "    Data [%p] \n",
           stack->size,
           stack->capacity,
           stack->data);
#endif

    if (stack->data == POISON_PTR or stack->data == nullptr)
    {
        printf("    Pointer poisoned.\n}\n");
        return;
    }
    for (size_t i = 0; i < stack->size; i++)
    {
        printf("    * [%zu] = %lg \n", i, stack->data[i]);
    }
    for (size_t i = stack->size; i < stack->capacity; i++)
    {
        printf("      [%zu] = %lg (POISON) \n", i, stack->data[i]);
    }
    printf("}\n");
}

size_t stackResizeMemory(Stack *stack, size_t newStackCapacity)
{
    size_t error = 0;
    Elem_t *newData =
        (Elem_t *) recalloc(stack->data,
                            sizeof(newData[0]) * stack->capacity,
                            sizeof(newData[0]) * newStackCapacity,
                            &error);
    if (newData == nullptr)
        return CANT_ALLOCATE_MEMORY_FOR_STACK;

    stack->data = newData;
    stack->capacity = newStackCapacity;

# if (HashProtection)
    stack->hash = stackHash(stack);
# endif

    ASSERT_OK(stack, &error)
    return error;
}

size_t stackResize(Stack *stack)
{
    assert(stack != nullptr);

    size_t error = NO_ERRORS;

    ASSERT_OK(stack, &error)

    if (error)
        return error;

    if (stack->capacity == 0)
    {
        error = stackResizeMemory(stack, 1);
        return error;
    }
    if (stack->size == stack->capacity)
    {
        size_t newStackCapacity = stack->capacity * 2;
        error = stackResizeMemory(stack, newStackCapacity);
        return error;
    }

    if (stack->size * 4 <= stack->capacity)
    {
        size_t newStackCapacity = stack->capacity / 2;
        error = stackResizeMemory(stack, newStackCapacity);
        return error;
    }

    ASSERT_OK(stack, &error)

    return error;
}

# if (HashProtection)
size_t hashData(void *data, size_t size)
{
    assert(data != nullptr);

    size_t hash = 5381;
    for (int i = 0; i < size; i++)
    {
        hash = 33 * hash + ((char *) data)[i];
    }
    return hash;
}

size_t stackHash(Stack *stack)
{
    assert(stack != nullptr);

    size_t old_hash = stack->hash;
    stack->hash = 0;
    size_t hash = hashData(stack, sizeof(*stack));
    stack->hash = old_hash;
    return hash;
}
# endif

