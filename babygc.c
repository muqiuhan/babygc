#include <stdbool.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdint.h>

#define INITIAL_GC_THRESHOLD 1024

#define assert(test, message)			\
  if (test) {					\
    puts(message);				\
    exit(EXIT_FAILURE);				\
  }

typedef enum
  {
    OBJ_INT,
    OBJ_PAIR
  } Object_Type ;

typedef struct Object
  {
    Object_Type type;
    bool marked;
    struct Object * next;
    
    union
      {
	int32_t value;

	struct
	  {
	    struct Object* head;
	    struct Object* tail;
	  };
      };
  } Object ;

#define STACK_MAX 256

typedef struct
  {
    Object * stack[STACK_MAX];
    Object * firstObject;
    uint32_t stackSize;

    uint32_t numObjects;
    uint32_t maxObjects;
  } VM ;

VM * newVM();
void push(VM * vm, Object * value);
Object * pop(VM *vm);
Object * newObject(VM * vm, Object_Type type);
void pushInt(VM * vm, int32_t intValue);
Object * pushPair(VM * vm);
void mark(Object * object);
void markAll(VM * vm);
void sweep(VM * vm);
void gc(VM * vm);

VM*
newVM()
  {
    VM * vm = malloc(sizeof(VM));
    vm -> stackSize = 0;
    vm -> firstObject = NULL;
    vm -> numObjects = 0;
    vm -> maxObjects = INITIAL_GC_THRESHOLD;
    return vm;
  }

void
push(VM * vm, Object * value)
  {
    assert(vm -> stackSize < STACK_MAX, "STACK OVERFLOW!!!");
    vm->stack[vm -> stackSize ++] = value;
  }

Object*
pop(VM *vm)
  {
    assert(vm -> stackSize > 0, "STACK UNDERFLOW!!!");
    return vm -> stack[--vm -> stackSize];
  }

Object*
newObject(VM * vm, Object_Type type)
  {
    if (vm -> numObjects == vm -> maxObjects) {
      gc(vm);
    }
    
    Object *object = malloc(sizeof(Object));
    object -> type = type;
    object -> marked = false;

    object -> next = vm -> firstObject;
    vm -> firstObject = object;
    vm -> numObjects ++;
  }

void
pushInt(VM * vm, int32_t intValue)
  {
    Object* object = newObject(vm, OBJ_INT);
    object -> value = intValue;
    push(vm, object);
  }

Object*
pushPair(VM * vm)
  {
    Object* object = newObject(vm, OBJ_PAIR);
    object -> tail = pop(vm);
    object -> head = pop(vm);

    push(vm, object);
    return object;
  }

void
mark(Object * object)
  {
    if (object -> marked) return;
    
    object -> marked = 1;

    if (object -> type == OBJ_PAIR) {
      mark(object -> head);
      mark(object -> tail);
    }
  }

void
markAll(VM * vm)
  {
    for (uint32_t i = 0; i < vm -> stackSize; i ++) {
      mark(vm -> stack[i]);
    }
  }

void
sweep(VM * vm)
  {
    Object ** object = &vm -> firstObject;

    while (*object) {
      if (!(*object) -> marked) {
	Object * unreached = *object;
	*object = unreached -> next;
	free(unreached);
	vm -> numObjects = (vm -> numObjects) - 1;
      } else {
	(*object) -> marked = false;
	object = &(*object) -> next;
      }
    }
  }

void
gc(VM * vm)
  {
    markAll(vm);
    sweep(vm);

    puts("call gc!");
  }

int
main(int argc, char *argv[])
  {
    VM * vm = newVM();
    
    for (int i = 0; i < INITIAL_GC_THRESHOLD * 10; i ++) {
      newObject(vm, OBJ_INT);
    }

    return 0;
  }
