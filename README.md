> 当我感到压力很大并且有太多事情要做时，我会产生一种自相矛盾的感觉，而我用另一件事情来麻痹这种感觉，一般是写个小程序......

# Introduction
在计算机科学中，垃圾收集器 (GC) 是一种自动内存管理方式。垃圾收集器尝试回收那些由程序分配但不再被引用的内存，这些内存就是垃圾。垃圾收集器是由计算机科学家约翰麦卡锡在 1959 年左右发明的，用于简化 Lisp 中的手动内存管理。

垃圾回收的基本原理是在程序中找到以后无法访问的数据对象，并回收这些对象使用的资源。

# Reduce, reuse, recycle
GC背后的基本思想是使用了GC的语言在大多数情况下可以访问无限多的内存，开发人员使用该语言可以一直分配，内存永远用不完。显然，机器没有无限多的内存，所以实现这种无限的内存的方式是：当需要分配内存而内存不足时，就使用GC收集垃圾释放掉，然后再分配内存。
先前说过，垃圾就是那些无用的内存，程序中的一个对象甚至是一个字节都可能有着至关重要的作用，所以垃圾收集器必须保证回收的精准，否则会出现未知的问题。同样的，当垃圾从成为垃圾到被回收的过程中，都必须保证程序中的其他有效的对象不能使用垃圾。

垃圾就是不再使用的对象，我们第一步要做的就是去锁定那些不再使用的对象，一般情况下有两种方案：
1. 在作用于内仍然存在某对象的引用在被使用的情况
2. 另一个正在使用的对象以及那个对象被其他任何正在使用的对象使用的情况

第一条比较好理解，而第二条并不是很清晰，其实第二条规则是递归的，即：如果对象A被对象B引用，那么A和B只要有一个是正在使用的，那么他们中的任何一个都不能被释放。这样一个一个对象互相引用环环相扣，最终会形成一个“可达对象图”，这个可达对象图展示了整个程序中的所有可用的对象，你可以从任何一个节点入手来遍历所有的对象，那么不在可达对象图中的对象就是垃圾了。 

# Marking and sweeping
有很多方案来实现垃圾回收，不过最简单的方案就是标题中给出的 “标记清除”， 这个方案也是麦卡锡发明的，它的工作原理几乎和上面的可达对象图的定义完全相同：
1. 从根部开始遍历整个对象图，将每一个遍历到的对象标记为正在使用
2. 完成后将所有未标记为正在使用的对象全部删除即可

# A pair of objects
在我们实现标记清除算法的GC之前，我们需要做一些准备工作，本文直接使用C语言而不会去实现一个解释器，虚拟机什么的题外之物，而是专注于GC。假设我们正在为一门语言编写解释器。它是动态类型的，并且有两种类型的对象：整数和对组(pair)。这是一个标识对象类型的枚举：
```c
typedef enum
  {
    OBJ_INT,
    OBJ_PAIR
  } Object_Type ;
```

pair可以是任何类型的组合，比如两个整数或者一个整数和一个对组，无论如何，目前就只有这俩类型给我们耍。接下来我们定义一个对象类型：
```c
typedef struct Object
  {
    Object_Type type;
    union
      {
	      int value;

	      struct
	        {
	          struct Object* head;
	          struct Object* tail;
	        };
      };
  } Object ;
```

Object结构有个type字段用来表示Object的类型，一个对象使用一个union来保存对象的数据。

# A minimal virtual machine
现在我们可以在一个小型虚拟机中使用刚刚定义的对象类型。虚拟机只需要创建一个栈来存储当前作用于内的变量。大多数语言的虚拟机要么基于栈（如 JVM 和 CLR），要么基于寄存器（如 Lua）。不管使用哪种虚拟机实现方案都会涉及到一个栈，它用于存储表达式处理时所需的局部变量和临时变量。我们简单的对虚拟机进行建模：
```c
typedef struct
  {
    Object* stack[STACK_MAX];
    int stackSize;
  } VM ;
```

这个模型非常简单，也可以很容易的创建一个这么简单的虚拟机：
```c
VM* newVM()
  {
    VM * vm = malloc(sizeof(VM));
    vm -> stackSize = 0;
    return vm;
  }
```

我们还需要能操作这个虚拟机的栈：
```c
void
push(VM * vm, Object * value)
  {
    assert(vm->stackSize < STACK_MAX, "STACK OVERFLOW!!!");
    vm->stack[vm->stackSize ++] = value;
  }

Object*
pop(VM *vm)
  {
    assert(vm->stackSize > 0, "STACK UNDERFLOW!!!");
    return vm->stack[--vm -> stackSize];
  }
```

有了栈就可以创建对象了，所以我们编写一个方便创建一个对象的辅助函数：
```c
Object*
newObject(VM * vm, Object_Type type)
  {
    Object *object = malloc(sizeof(Object));
    object -> type = type;
  }
```

这还不够，因为我们需要将每一种对象都push到栈中，所以要在push之上再抽象一层:
```c
void
pushInt(VM * vm, int intValue)
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
```

接下来拥有了制造垃圾的条件，可以开始批量产出垃圾了。

# Marky mark
标记清除算法的第一步就是标记，我们需要遍历所有可达对象并标记它们，所以现在得先向Object添加一个标记字段：
```c
typedef struct Object
  {
    Object_Type type;
    bool marked;
    
    union
      {
	    int value;

        struct
	      {
	        struct Object* head;
	        struct Object* tail;
	      };
      };
  } Object ;
```

可以看到，这里添加了一个`bool marked`，用于标记对象是否处在可达对象图中，当我们使用`newObject()`创建一个新的对象时，就将这个新对象的`marked`设置为false，因为刚创建的对象并没有被任何对象引用，它并不存在可达对象图中，是垃圾。为了标记所有可达对象，我们需要遍历栈，不过在此之前，我们先实现一个`mark()`函数用于将对象的`marked`标记设置为true:
```c
void mark(Object * object)
  {
    object -> marked = 1;
  }
```
从字面上看我们可以通过mark字段将对象标记为可访问，直观上来讲就是将这个对象放入了可达对象图中，但是这还不够，因为还有引用的情况 —— 可达性是递归的，如果对象是一个pair，那么它的另外两个字段的`marked`也要标记为`true`，这写起来不难：
```c
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
```

`mark()`函数的递归边界是一个`if (object -> marked) return;`的检查，它检查对象是否被标记，如果是的话就停止递归。

接下来可以实现`markAll()`函数，这个函数会标记所有的可达对象，所以它其实就是遍历可达对象图并标记访问到的对象:
```c
void
markAll(VM * vm)
  {
    for (int i = 0; i < vm -> stackSize; i ++) {
      mark(vm -> stack[i]);
    }
  }
```

# Sweepy sweep
下一步就是扫描我们创建的所有对象并且释放那些`marked`字段为false的对象，但是这里有一个问题：根据定义来讲，所有`marked == false`的对象都不在可达对象图内，也就是说这些对象都访问不到，那我们也就无法释放他们。

要解决这个问题，我们需要让虚拟机拥有自己的对象引用，这些对象与外界不同，我们可以自己跟踪它们。最简单的一个实现方式是用一个链表保存所有的对象，所以我们只需要让每个对象成为一个节点即可：
```c
typedef struct Object
  {
    Object_Type type;
    bool marked;
    struct Object * next;
    
      union
        {
	      int value;

	      struct
	        {
	          struct Object* head;
	          struct Object* tail;
	        };
        };
  } Object ;
```

这里我们添加了一个`struct Object * next;`用于让一个对象指向其他的对象，这样串成一个链表，而虚拟机只需要保存这个链表的头部即可：
```c
typedef struct
  {
    Object * stack[STACK_MAX];
    Object * firstObject;
    int stackSize;
  } VM ;
```
即`Object * firstObject;`，同时更改 `newVM()`，确保将`firstObject`初始化为NULL。并且每当我们创建一个对象时，都将它添加到链表中：
```c
VM*
newVM()
  {
    VM * vm = malloc(sizeof(VM));
    vm -> stackSize = 0;
    vm -> firstObject = NULL;
    return vm;
  }
```

```c
Object*
newObject(VM * vm, Object_Type type)
  {
    Object *object = malloc(sizeof(Object));
    object -> type = type;
    object -> marked = false;

    object -> next = vm -> firstObject;
    vm -> firstObject = object;
  }
```

这么做的话，虚拟机就可以跟踪所有对象了，就算对象不在可达对象图中，也在虚拟机的对象表中。

接下来我们遍历这个链表（虚拟机对象表）：
```c
void
sweep(VM * vm)
  {
    Object ** object = &vm -> firstObject;

    while (*object) {
      if (!(*object) -> marked) {
		Object * unreached = *object;
		*object = unreached -> next;
		free(unreached);
      } else {
		(*object) -> marked = false;
		object = &(*object) -> next;
      }
    }
  }
```

`sweep()`函数遍历整个链表。每当它碰到一个未标记的对象(`marked = false`)时，它就会释放其内存并将其从列表中删除。

现在我们将`markAll()`和`sweep()`给包装起来，交给外界一个gc:
```c
void
gc(VM * vm)
  {
    markAll(vm);
    sweep(vm);
  }
```

接下来，我们通过增加`numObjects`和`maxObjects`字段到VM，让VM规定一个最大的对象数量，如果到达这个数量就启动GC:
```c
typedef struct
  {
    Object * stack[STACK_MAX];
    Object * firstObject;
    int stackSize;

    int numObjects;
    int maxObjects;
  } VM ;
```

`numObjects`是当前的对象数量，`maxObjects`是最大的对象数量，这里先将其初始化为0
```c
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
```

`INITIAL_GC_THRESHOLD` 是对象数量的最大值，这个值如果太小的话内存的使用会更加保守，太大的话垃圾收集上花费的时间更少（因为GC频率会降低），自己调整口味即可。

每当我们创建一个对象时，就将虚拟机的`numObjects`加一，当`numObjects == maxObjects`时就触发gc:
```c
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
```

也要记住在`sweep()`函数内每次释放一个对象的时候将VM的`numObjects`减一：
```c
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
```
即`vm -> numObjects = (vm -> numObjects) - 1;`

然后我们向`gc()`函数中添加一个打印语句来提示我们gc被调用了:
```c
void
gc(VM * vm)
  {
    markAll(vm);
    sweep(vm);

    puts("call gc!");
  }
```

# 测试
```c
int
main(int argc, char *argv[])
  {
    VM * vm = newVM();
    
    for (int i = 0; i < INITIAL_GC_THRESHOLD * 10; i ++) {
      newObject(vm, OBJ_INT);
    }

    return 0;
  }
```

运行结果:
```shell
$ ./a.out
call gc!
call gc!
call gc!
call gc!
call gc!
call gc!
call gc!
call gc!
call gc!
```

# Reference
- http://journal.stuffwithstuff.com/2013/12/08/babys-first-garbage-collector/
- https://en.wikipedia.org/wiki/Garbage_collection_(computer_science)
