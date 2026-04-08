# Python 与 pybind11 底层原理可视化教程

> 面向 visual learner：用“问题 → 思路 → 结构图 → 效果 → 自己重新发明一遍”的方式理解。  
> 说明：下面说的“Python 底层”默认主要指 **CPython**，因为我们今天常用的 Python 官方实现就是它；而 pybind11 本质上是构建在 **CPython C API** 之上的一层现代 C++ 薄封装。

---

# 0. 先建立全局地图

把整件事先看成三层：

```text
┌──────────────────────────────┐
│ 你写的 Python 代码            │
│ 例: x = add(1, 2)             │
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐
│ CPython 解释器                │
│ 词法/语法 → AST → 字节码 → VM  │
│ 对象模型 / 引用计数 / GC / GIL │
└──────────────┬───────────────┘
               │ C API
               ▼
┌──────────────────────────────┐
│ pybind11                     │
│ 把 C++ 函数/类包装成 Python 对象│
│ 并在 Python <-> C++ 间做转换   │
└──────────────┬───────────────┘
               │
               ▼
┌──────────────────────────────┐
│ 你的 C++ 代码                 │
│ 高性能算法 / 库 / 系统代码     │
└──────────────────────────────┘
```

一句话：

- **Python / CPython** 解决的是：怎样执行 Python 语言。
- **pybind11** 解决的是：怎样让 Python 世界和 C++ 世界安全、自然地互相调用。

---

# 1. 基本问题 1：Python 到底是什么？

很多人会把“Python 语言”和“Python 运行时”混在一起。

## 1.1 语言 vs 实现

```text
Python 语言规范
    ↓ 可以有多种实现
CPython / PyPy / Jython / MicroPython ...
```

你平时安装的 `python3`，大概率是：

- **Python 语言**：语法规则、语义规则
- **CPython**：最主流实现，用 C 写的解释器

所以讨论“Python 底层原理”，大多数场景其实是在讨论：

- CPython 如何表示对象
- CPython 如何执行代码
- CPython 如何管理内存
- CPython 如何暴露 C API

---

# 2. 基本问题 2：Python 代码是怎么跑起来的？

先看一个简单例子：

```python
x = 1 + 2
print(x)
```

## 2.1 从源码到执行的流水线

```text
源码字符串
   │
   ▼
词法分析 (tokenize)
   │
   ▼
语法分析 (parse)
   │
   ▼
AST 抽象语法树
   │
   ▼
编译成字节码 bytecode
   │
   ▼
虚拟机 VM 逐条执行字节码
```

可以把它想成：

- **源码**：人看的文本
- **AST**：结构化语法树
- **字节码**：更接近机器执行但仍是平台无关的中间指令
- **VM**：执行这些指令的机器

## 2.2 可视化：`x = 1 + 2`

### 源码层

```text
x = 1 + 2
```

### AST 层（示意）

```text
Assign
├── Name("x")
└── BinOp(+)
    ├── Constant(1)
    └── Constant(2)
```

### 字节码层（概念示意）

```text
LOAD_CONST 1
LOAD_CONST 2
BINARY_ADD
STORE_NAME x
```

### 执行效果

```text
栈: []
LOAD_CONST 1   -> [1]
LOAD_CONST 2   -> [1, 2]
BINARY_ADD     -> [3]
STORE_NAME x   -> []   且 x = 3
```

这说明：**CPython 的执行核心，很像一台“基于栈的虚拟机”。**

---

# 3. 基本问题 3：为什么 Python 看起来“什么都是对象”？

因为在 CPython 里，几乎一切值都被包装成统一的对象结构。

这里的“对象”，不要先把它想成面向对象编程里的 class instance，先把它想成：

```text
一块运行时可识别的内存
= 对象头 + 类型信息 + 具体数据
```

也就是说，解释器眼里：

- `42` 是对象
- `"hello"` 是对象
- `[1, 2, 3]` 是对象
- 函数是对象
- 类本身也是对象
- 模块也是对象

---

## 3.1 变量名不是盒子，而是“名字 -> 对象引用”

你写：

```python
a = 42
b = a
```

更接近底层的图像是：

```text
名字表 / 命名空间
├── "a" ─────► PyLongObject(42)
└── "b" ─────► 同一个 PyLongObject(42)
```

所以 Python 赋值通常不是“复制对象本体”，而是：

- 把一个名字绑定到对象
- 或把另一个名字也绑定到同一个对象

这就是为什么下面现象成立：

```python
x = [1, 2]
y = x
y.append(3)
print(x)   # [1, 2, 3]
```

视觉上：

```text
x ─┐
   ├──► PyListObject [...]
y ─┘
```

`x` 和 `y` 不是两个 list，只是两个名字指向同一个 list 对象。

---

## 3.2 最小对象头：解释器先看“你是谁”

CPython 里很多对象都共享一个“对象头”概念：

```text
PyObject
├── 引用计数 ob_refcnt
└── 类型指针 ob_type
```

可以把它理解成每个对象出厂都带两张身份证：

- `ob_refcnt`：现在有多少地方引用我
- `ob_type`：我到底是什么类型

### 视觉化

```text
             ┌─────────────────────┐
对象起始地址 ─► ob_refcnt = 3       │
             ├─────────────────────┤
             │ ob_type = &PyLong   │
             ├─────────────────────┤
             │ 下面才是具体数据区   │
             └─────────────────────┘
```

这带来两个关键能力：

1. 解释器拿到任意对象指针，都能先查类型。  
2. 解释器能根据类型决定允许什么操作。  

---

## 3.3 定长对象 vs 变长对象

不是所有对象都一样大。

### 定长对象

例如一个简单对象，头部之后的数据大小固定。

### 变长对象

例如：

- `int`：数字可能有很多“位”
- `str`：字符数可变
- `tuple`：元素个数可变
- `list`：元素个数可变

CPython 为这种对象引入了一个常见概念：

```text
PyVarObject
├── PyObject 头
└── ob_size   # 逻辑大小 / 元素数 / 位数（具体含义依类型而定）
```

所以可以先粗略记：

```text
PyObject    = 最小公共头
PyVarObject = 最小公共头 + “长度/规模”信息
```

这不是说所有变长对象都只靠 `ob_size` 就够了，而是说它们通常会以这个公共前缀为基础，再接自己的专用字段。

---

## 3.4 不同对象，头一样，身体不一样

下面是几个典型对象的直觉图。

### 1) 整数对象 `PyLongObject`

```text
PyLongObject
├── ob_refcnt
├── ob_type = &PyLong_Type
├── ob_size
└── digits[]   # 真正存数值的“多精度位块”
```

这说明 Python 的整数不是固定 32 位或 64 位，而是“按需要变长”的。

所以：

```python
10 ** 1000
```

在 Python 里依然是普通整数对象，只是底层 digits 数组更长。

### 2) 字符串对象 `PyUnicodeObject`

概念上可以先理解成：

```text
PyUnicodeObject
├── 对象头
├── 长度信息
├── 编码/状态信息
└── 字符数据区
```

它不是简单的 `char*`，因为 Python 3 要很好支持 Unicode。

### 3) 列表对象 `PyListObject`

```text
PyListObject
├── 对象头
├── ob_size        # 现在有多少个元素
├── ob_item        # 指向“元素指针数组”
└── allocated      # 当前预留容量
```

注意：list 里存的通常不是元素本体，而是**元素对象指针**。

```python
lst = [10, "hi", [1, 2]]
```

视觉化：

```text
lst ───► PyListObject
         ├── ob_size = 3
         ├── allocated = 4
         └── ob_item ─────► [ ptr, ptr, ptr, ... ]
                              │    │    │
                              ▼    ▼    ▼
                           int   str   list
```

所以 list 像一个“对象指针数组”，这也是它能装不同类型元素的根本原因。

---

## 3.5 Python 对象不是“值”，而是“对象壳 + 指向别的对象”层层相连

看：

```python
x = [1, 2, 3]
```

很多初学者脑中图像是：

```text
x = [1, 2, 3]
```

但底层更像：

```text
x ───────────────► PyListObject
                   └── ob_item ─► [ p0, p1, p2 ]
                                  │   │   │
                                  ▼   ▼   ▼
                                 int int int
```

也就是说：

- list 对象自己是一层壳
- 它内部再引用其他对象
- 对象图会形成网状结构，而不是扁平值

这也是为什么 Python 的内存管理会和“对象图”强相关。

---

## 3.6 `ob_type` 不只是标签，它还连接到“行为定义”

很多人以为类型指针只是为了打印名字，其实远不止。

`ob_type` 背后连的是类型对象，而类型对象里有大量“这个类型该怎么行为”的定义。

可以把类型对象想成：

```text
TypeObject
├── 类型名
├── 对象大小
├── 构造/析构逻辑
├── 打印逻辑
├── 加法逻辑
├── 比较逻辑
├── 下标访问逻辑
└── 方法表 / 槽位表
```

所以解释器看到一个对象时，常见流程是：

```text
拿到对象指针
   │
   ▼
看 ob_type
   │
   ▼
去类型对象里找这类对象支持哪些操作
```

这就是 Python 动态性的底层来源之一。

---

## 3.7 为什么 `+`、`[]`、调用 `()` 都能统一落到对象模型上

例如：

```python
1 + 2
s[0]
f(123)
```

表面是语法，底层都可以理解为：

```text
对象 + 类型对象里的操作入口
```

比如：

- `1 + 2`：去 `int` 类型的“加法槽位”
- `s[0]`：去 `str` 或 `list` 类型的“下标槽位”
- `f(123)`：检查对象类型是否可调用，并走调用入口

这很像：

```text
语法糖
   ↓
运行时分派到类型实现
```

所以“对象模型”不是 Python 里一个孤立概念，而是整个语言执行模型的中心。

---

## 3.8 可变对象 vs 不可变对象，底层意义是什么

### 不可变对象

例如：

- `int`
- `str`
- `tuple`

你写：

```python
x = 10
x = x + 1
```

不是把原来那个 `10` 对象原地改成 `11`，而更像：

```text
旧名字 x ─► int(10)
计算 x + 1
创建新对象 int(11)
让 x 改绑到新对象
```

### 可变对象

例如：

- `list`
- `dict`
- `set`

你写：

```python
x = [1, 2]
x.append(3)
```

更像：

```text
同一个 PyListObject
其内部元素指针数组被修改
```

### 视觉对比

#### 不可变：重绑名字

```text
x ─► int(10)
      ↓ x = x + 1
x ─► int(11)
```

#### 可变：改对象内部状态

```text
x ─► list([1,2])
      ↓ append(3)
x ─► same list([1,2,3])
```

这就是为什么“共享可变对象”会带来副作用，而共享不可变对象通常没问题。

---

## 3.9 类型对象本身也是对象：`type` 也是运行时的一部分

这点很 Python。

你可以这样想：

```text
对象有类型
类型本身也是对象
```

例如：

```python
type(123)
type("hi")
```

这些返回的也都是对象。

概念图：

```text
123 ───────────► PyLongObject
                  │
                  ▼
               PyLong_Type
                  │
                  ▼
                 type
```

这让 Python 的类型系统非常“运行时化”：

- 类可以在运行时创建
- 类也能被传来传去
- 元类也能参与行为定义

对于初学底层原理的人来说，先记住一句就够：

**实例是对象，类也是对象，类型系统本身也在对象模型里。**

---

## 3.10 特别补充：`class` 到底是什么对象？

这是理解 Python object model 最容易“突然开窍”的地方。

很多语言里，类更像编译期概念；但在 Python 里，`class` 本身就是一个**运行时对象**。

先看：

```python
class Dog:
    kind = "animal"

    def bark(self):
        return "wang"
```

你可以把它想成：

```text
执行 class 语句
   │
   ▼
创建一个类对象 Dog
   │
   ├── 里面有类名
   ├── 有属性字典
   ├── 有方法定义
   ├── 有基类信息
   └── 它自己的类型通常是 type
```

也就是说：

- `Dog` 不是“语法标签”
- `Dog` 是一个真正的对象
- 这个对象负责描述“Dog 实例应该长什么样、会什么行为”

### 3.10.1 类对象、实例对象、`type` 三者关系

看：

```python
class Dog:
    pass

d = Dog()
```

概念图：

```text
d ─────────────► Dog 的一个实例对象
                  │
                  ▼
                 Dog
                  │
                  ▼
                 type
```

也就是：

- `d` 的类型是 `Dog`
- `Dog` 的类型通常是 `type`

所以：

```python
print(type(d))     # <class '__main__.Dog'>
print(type(Dog))   # <class 'type'>
```

这个结构很关键，因为它说明 Python 的类系统本身就在对象系统内部，不在对象系统外面。

---

### 3.10.2 `class` 语句本质上是在“构造一个类对象”

你写：

```python
class Dog:
    x = 1

    def bark(self):
        return "wang"
```

可以把它粗略理解成：

```text
1. 先准备一个临时命名空间 dict
2. 执行 class 代码块，把 x、bark 放进去
3. 调用 metaclass（默认通常是 type）
4. 产出类对象 Dog
```

视觉化：

```text
class body 执行后得到 namespace
{
  "x": 1,
  "bark": <function bark>
}
          │
          ▼
     type("Dog", bases, namespace)
          │
          ▼
        类对象 Dog
```

所以 class body 不是“声明式文本”，而是真的会执行。

这就是为什么类定义里可以出现可执行语句。

---

### 3.10.3 类对象里面最重要的东西之一：属性字典

对初学者，可以先把类对象想成：

```text
ClassObject Dog
├── __name__ = "Dog"
├── __bases__ = (...)
├── __dict__
│   ├── "kind" -> "animal"
│   └── "bark" -> function object
└── 其他类型系统相关字段
```

所以：

```python
Dog.kind
Dog.bark
```

本质上都是从类对象里查属性。

实例对象也常常有自己的属性存储，例如：

```python
class Dog:
    def __init__(self, name):
        self.name = name
```

创建实例：

```python
d = Dog("Milo")
```

概念图：

```text
实例对象 d
├── __dict__
│   └── "name" -> "Milo"
└── ob_type -> Dog

类对象 Dog
├── __dict__
│   ├── "__init__" -> function
│   ├── "bark" -> function
│   └── "kind" -> "animal"
└── type -> type
```

---

### 3.10.4 属性查找：为什么 `d.name`、`d.kind`、`d.bark()` 都能工作

看：

```python
class Dog:
    kind = "animal"
    def bark(self):
        return "wang"

    def __init__(self, name):
        self.name = name

d = Dog("Milo")
```

#### 查 `d.name`

先看实例自己的属性区：

```text
d.__dict__ 里有 "name"
=> 直接返回
```

#### 查 `d.kind`

先看实例自己的属性区，没有。  
于是去类对象 `Dog` 上找：

```text
d.__dict__ 没有
   ↓
Dog.__dict__ 里找到 "kind"
   ↓
返回 "animal"
```

#### 查 `d.bark`

过程更特别：

```text
d.__dict__ 没有
   ↓
Dog.__dict__ 找到 function bark
   ↓
把 function 和当前实例 d 绑定
   ↓
得到 bound method
```

也就是说：

```python
d.bark()
```

底层精神更像：

```python
Dog.bark(d)
```

这就是 `self` 的来历：不是关键字魔法，而是方法绑定机制的结果。

---

### 3.10.5 为什么函数放进 class 后，会变成 method

单独函数：

```python
def f(x):
    return x
```

它只是函数对象。

但当函数出现在类的字典里，再通过实例访问时：

```python
d.bark
```

解释器会触发描述符协议/方法绑定逻辑，把它包装成“绑定方法”。

概念图：

```text
Dog.__dict__["bark"] = function object
            │
通过实例 d 访问
            ▼
bound method
├── __func__ -> 原始 function
└── __self__ -> 当前实例 d
```

所以 method 不是一种完全独立写出来的东西，而往往是“函数对象在类访问语境下绑定后的产物”。

---

### 3.10.6 实例是怎么创建出来的

看：

```python
d = Dog("Milo")
```

可以粗略想成：

```text
调用类对象 Dog
   │
   ▼
Dog.__call__ / type 的调用逻辑
   │
   ├── 先分配一个新实例对象
   ├── 再调用 __init__(self, ...)
   └── 返回这个实例
```

也就是：

- 类对象本身通常是可调用的
- “调用类”其实是在造实例

视觉化：

```text
Dog("Milo")
   │
   ▼
[创建空实例 d]
   │
   ▼
Dog.__init__(d, "Milo")
   │
   ▼
返回 d
```

这也是为什么类对象既像“模板”，又像“工厂”。

---

### 3.10.7 继承在对象模型里是什么

看：

```python
class Animal:
    def speak(self):
        return "..."

class Dog(Animal):
    pass
```

底层最重要的一点不是语法，而是：

```text
Dog 知道自己的基类是 Animal
属性查找失败时，会沿着继承链往上找
```

视觉化：

```text
实例 d
  │
  ▼
类 Dog
  │
  ▼
基类 Animal
  │
  ▼
object
```

当 `d.speak` 查不到自身和 `Dog` 时，就继续去 `Animal` 上找。

这就是继承在对象模型里的本质：**查找路径扩展**。

---

### 3.10.8 `object` 和 `type` 为什么特别重要

这两个是 Python 类系统里最核心的两个内建类型。

#### `object`

- 大多数新式类的最终基类
- 提供最基础对象行为

#### `type`

- 大多数类对象的类型
- 负责“创建类对象”

最经典的记忆图：

```text
实例对象 的类型 是 某个类
某个类对象 的类型 是 type
大多数类对象 的基类链 最后到 object
```

简化图：

```text
d -> Dog -> type
Dog -> object   （从继承关系角度）
```

注意这是两条不同维度：

- 一条是“谁是谁的类型”
- 一条是“谁继承谁”

这是初学者最容易混的点。

---

### 3.10.9 一个总图：`class` 在 Python object system 里的位置

```text
实例对象 d
├── 实例属性 (__dict__)
└── ob_type ─────────────► 类对象 Dog
                           ├── 类属性 / 方法 (__dict__)
                           ├── __bases__ = (Animal,)
                           └── ob_type ─► type

Animal 也同样是类对象
object 也是类对象
type 也是对象
```

一句话压缩：

**class 在 Python 里不是“编译期模板”，而是一个运行时对象；实例通过 `ob_type` 指向类对象，类对象再通过 `type` 纳入整个对象系统。**

---

## 3.11 一个“自己重新发明 Python object”的最小设计

如果你要从零实现一个玩具版 Python 对象系统，最小可以先做成这样：

```c
typedef struct TypeObject TypeObject;

typedef struct {
    int refcnt;
    TypeObject* type;
} Object;

struct TypeObject {
    const char* name;
    Object* (*add)(Object* a, Object* b);
    Object* (*getitem)(Object* obj, Object* index);
    void (*print)(Object* obj);
};
```

然后做几个具体对象：

```c
typedef struct {
    Object base;
    long value;
} IntObject;

typedef struct {
    Object base;
    int size;
    Object** items;
} ListObject;
```

这样你立刻就能模拟很多 Python 精神：

- 每个对象都知道自己类型
- 操作交给类型实现
- list 里存 `Object*`，所以能混装
- 解释器只需要围绕 `Object*` 工作

执行 `a + b` 的时候：

```c
Object* result = a->type->add(a, b);
```

执行 `obj[i]` 的时候：

```c
Object* result = obj->type->getitem(obj, i);
```

这就是“统一对象模型”的力量：

```text
解释器本体不用知道所有类型细节
它只需要遵守公共协议
```

---

## 3.11 一个“自己重新发明 Python object”的最小设计

```text
名字/变量
   │
   ▼
Object* 指针
   │
   ▼
┌───────────────────────────┐
│ PyObject / PyVarObject 头  │
│ - ob_refcnt               │
│ - ob_type                 │
│ - (maybe) ob_size         │
└─────────────┬─────────────┘
              │
              ▼
     具体类型自己的数据区
     ├── int digits
     ├── str chars/data
     ├── list item pointers
     └── function / class / module state
              │
              ▼
        类型对象 TypeObject
        ├── 名字
        ├── 大小
        ├── 方法/槽位
        └── 行为定义
```

一句话压缩：

**Python object = 统一对象头 + 类型信息 + 类型专属数据 + 通过类型对象获得行为。**

---

# 4. Python 的对象类型都有哪些？

严格说，没有一个“封顶的固定列表”，因为：

- Python 里用户可以自己定义新类
- 第三方库也可以引入扩展类型
- 从运行时角度看，类本身、函数本身、模块本身也都是对象

但为了建立地图，可以先把常见对象类型分成几大类。

## 4.1 按学习视角分的总图

```text
Python objects
├── 基础标量对象
├── 文本与二进制对象
├── 容器对象
├── 可调用对象
├── 类与类型系统对象
├── 运行时内部对象
├── 特殊单例对象
└── 用户自定义 / 扩展对象
```

---

## 4.2 基础标量对象

这些对象通常最先接触：

- `NoneType`
- `bool`
- `int`
- `float`
- `complex`

例子：

```python
x = None
a = True
b = 123
c = 3.14
d = 2 + 3j
```

它们的共同点是：

- 通常看起来像“单个值”
- 多数是不可变对象
- 在 CPython 里也都有对应的具体对象结构

视觉上：

```text
123      -> int object
3.14     -> float object
True     -> bool object
None     -> None singleton object
```

---

## 4.3 文本与二进制对象

这组对象解决“字符”和“原始字节”问题：

- `str`
- `bytes`
- `bytearray`
- `memoryview`

### 直觉区分

- `str`：文本，面向字符/Unicode
- `bytes`：不可变字节序列
- `bytearray`：可变字节序列
- `memoryview`：对底层内存的一层视图，不一定复制

例子：

```python
s = "hello"
b = b"hello"
ba = bytearray(b"hello")
mv = memoryview(b"hello")
```

---

## 4.4 容器对象

最常见的一组：

- `list`
- `tuple`
- `dict`
- `set`
- `frozenset`
- `range`
- `slice`

### 它们的底层意义

- `list`：对象指针数组，可变
- `tuple`：对象指针数组，不可变
- `dict`：键值映射表
- `set`：去重集合
- `frozenset`：不可变集合
- `range`：一种紧凑的区间对象，不是预先展开的 list
- `slice`：切片规则对象

例子：

```python
lst = [1, 2, 3]
tup = (1, 2, 3)
d = {"x": 1}
s = {1, 2, 3}
r = range(10)
sl = slice(1, 5, 2)
```

---

## 4.5 可调用对象

“能被 `()` 调用”的都属于这一大类，但内部并不只有函数。

常见包括：

- `function`
- `builtin_function_or_method`
- `method`
- `lambda`（本质仍是 function）
- `generator`
- `coroutine`
- `async_generator`
- 实现了 `__call__` 的类实例

例子：

```python
def f(x):
    return x + 1

class C:
    def __call__(self, x):
        return x * 2
```

这里：

- `f` 是函数对象
- `C()` 创建的是实例对象
- 但这个实例因为定义了 `__call__`，所以也可调用

所以“可调用”是行为，不一定是“函数类型”。

---

## 4.6 特别补充：`class` 属于什么对象？

`class` 最值得单独记住。

在 Python 里：

- 类本身是对象
- 类对象通常是 `type` 的实例
- 类对象保存类属性、方法、基类信息
- 实例对象通过自己的 `ob_type` 指向类对象

例如：

```python
class Dog:
    pass

d = Dog()
```

关系图：

```text
d 是实例对象
Dog 是类对象
Dog 的类型通常是 type
```

也就是：

```python
type(d) is Dog
type(Dog) is type
```

如果你脑中能稳定记住这一句，class 相关的大多数困惑都会少很多：

**Python 的 class 不是编译期模板，而是运行时对象。**

---

## 4.7 类与类型系统对象

除了普通类对象，这一组里还有：

- `object`
- `type`
- `super`
- `property`
- `classmethod`
- `staticmethod`

### 它们在干什么

- `object`：大多数类的最终基类
- `type`：大多数类对象的类型，也是创建类对象的核心元类
- `super`：帮助沿继承链查找方法
- `property`：把方法包装成属性访问协议
- `classmethod` / `staticmethod`：改变函数在类中的绑定方式

这组对象说明：**Python 的类机制本身也是通过对象协作完成的。**

---

## 4.8 运行时内部对象

这一组平时不一定天天写，但理解底层时很重要：

- `module`
- `code`
- `frame`
- `traceback`
- `cell`

### 直觉作用

- `module`：模块本身也是对象，有自己的命名空间
- `code`：编译后的代码对象，保存字节码和元数据
- `frame`：一次函数调用的执行现场
- `traceback`：异常传播路径
- `cell`：闭包捕获变量用到的对象

例如闭包：

```python
def outer():
    x = 10
    def inner():
        return x
    return inner
```

这里 `x` 被内部函数捕获时，底层会涉及 cell object。

---

## 4.9 特殊单例对象

这组对象数量少，但语义很特殊：

- `None`
- `Ellipsis`（`...`）
- `NotImplemented`

### 它们不是关键字魔法值，而是真对象

例如：

```python
x is None
... is Ellipsis
```

可以把它们想成：

```text
解释器预先提供的全局唯一特殊对象
```

特别是：

- `None`：表示“没有值”
- `NotImplemented`：常出现在双目运算协商里
- `Ellipsis`：切片、占位等场景可见

---

## 4.10 异常对象

异常不是“字符串报错”，而是对象。

例如：

```python
raise ValueError("bad value")
```

这里实际创建的是一个异常实例对象。

关系图：

```text
ValueError 是类对象
ValueError("bad value") 是异常实例对象
```

这也是为什么异常可以：

- 有类型层次
- 带字段和消息
- 被捕获时按类型匹配

---

## 4.11 用户自定义对象

你写一个类：

```python
class User:
    def __init__(self, name):
        self.name = name
```

然后：

```python
u = User("Ada")
```

那么 `u` 就是一个用户自定义对象。

它和内建对象在对象模型层面并没有本质区别：

- 都是对象
- 都有类型
- 都参与属性查找和引用计数

只是它的类型不再是 `int`、`list` 这种内建类，而是你定义的 `User`。

---

## 4.12 扩展对象：第三方库把新对象类型接进 Python

比如：

- `numpy.ndarray`
- `pandas.DataFrame`
- `torch.Tensor`

这些也都是 Python 对象。

区别在于：

- 它们经常不是纯 Python 写的
- 底层往往由 C/C++/Rust 扩展实现
- 但最终都会接入 Python 的对象模型

也就是说，在 Python 眼里它们和内建对象一样，都必须表现得像对象：

- 有类型
- 能引用计数
- 能参与属性访问/方法调用

这也正是 pybind11 这类工具存在的原因。

---

## 4.13 从 CPython 底层名字看常见对象

如果你从 C API / pybind11 角度学习，常见底层对象名大概会长这样：

- `int` -> `PyLongObject`
- `float` -> `PyFloatObject`
- `str` -> `PyUnicodeObject`
- `bytes` -> `PyBytesObject`
- `list` -> `PyListObject`
- `tuple` -> `PyTupleObject`
- `dict` -> `PyDictObject`
- `set` -> `PySetObject`
- `function` -> `PyFunctionObject`
- `module` -> `PyModuleObject`
- `type` -> `PyTypeObject`
- `code` -> `PyCodeObject`
- `frame` -> `PyFrameObject`
- `generator` -> `PyGenObject`

不用一口气全背，先知道：

```text
Python 层看到的是 int/list/function/class
CPython 层看到的是 PyLongObject/PyListObject/PyFunctionObject/PyTypeObject
```

---

## 4.14 最值得先记住的 6 大类

如果你现在只想建立最稳的认知框架，先记这 6 类就够：

1. 标量对象：`int`、`float`、`bool`、`str`  
2. 容器对象：`list`、`tuple`、`dict`、`set`  
3. 可调用对象：函数、方法、生成器、协程  
4. 类对象：用户定义类、内建类、`type`  
5. 运行时对象：`module`、`code`、`frame`  
6. 特殊对象：`None`、异常对象、第三方扩展对象  

一句话压缩：

**Python 的对象类型不是只有“数据类型”，还包括函数、类、模块、异常、代码对象、栈帧对象，整个运行时几乎都建立在对象之上。**

---

# 5. Python 的对象模型：为什么 `+` 能加整数也能拼字符串？

看下面代码：

```python
1 + 2
"a" + "b"
```

表面上同一个 `+`，底层不是同一个函数。

## 4.1 底层思路：操作交给类型

```text
BINARY_ADD
   │
   ▼
查看左操作数对象的类型
   │
   ▼
去类型槽位 / 方法表里找“加法实现”
   │
   ├── int 的加法
   ├── str 的拼接
   └── list 的拼接
```

所以 Python 的动态性，本质上来自：

- 运行时对象都带类型信息
- 操作不是写死在语法里，而是委托给对象类型

## 4.2 一个“自己发明 Python 对象系统”的极简模型

```c
typedef struct TypeObject TypeObject;

typedef struct {
    int refcnt;
    TypeObject* type;
} Object;

struct TypeObject {
    const char* name;
    Object* (*add)(Object* a, Object* b);
    void (*print)(Object* obj);
};
```

这时，解释器执行 `a + b` 时就可以做：

```c
Object* result = a->type->add(a, b);
```

这已经很像 Python 的动态分派精神了。

---

# 5. Python 的内存管理：引用计数为什么直观又危险？

## 5.1 最核心机制：引用计数

每个对象记录“有多少地方正在引用我”。

```text
对象 A: refcnt = 1
x = A        -> refcnt = 1
y = x        -> refcnt = 2
del x        -> refcnt = 1
del y        -> refcnt = 0 -> 释放
```

## 5.2 视觉化

```text
初始
x ─────► [Object A | refcnt=1]

赋值给 y 后
x ─┐
   ├──► [Object A | refcnt=2]
y ─┘
```

## 5.3 为什么它好用？

优点：

- 对象通常能**立即释放**，时机可预测。
- 实现概念直观。
- C 扩展容易接入：拿到对象就 incref，用完 decref。

## 5.4 为什么它不够？

因为**循环引用**。

```python
a = []
b = []
a.append(b)
b.append(a)
```

即使外部名字都删除：

```python
del a
del b
```

内部仍然互相引用：

```text
Object A(refcnt=1) <----> Object B(refcnt=1)
```

所以单靠引用计数，这俩永远不会变成 0。

## 5.5 于是 CPython 还需要循环垃圾回收器

思路不是替代引用计数，而是**补洞**：

- 平时主要靠引用计数
- 周期性检测容器对象之间是否形成“孤岛循环”
- 如果这团对象从根不可达，就回收

这个设计很工程化：

```text
常见情况：引用计数快速处理
特殊情况：GC 处理循环
```

---

# 6. Python 的执行核心：字节码虚拟机 VM

## 6.1 VM 是什么

可以把 VM 想成一个大循环：

```c
while (1) {
    opcode = fetch_next_instruction();
    switch (opcode) {
        case LOAD_CONST: ...
        case BINARY_ADD: ...
        case CALL: ...
    }
}
```

本质就是：

- 取一条指令
- 修改栈/局部变量/程序计数器
- 再取下一条

## 6.2 为什么是“栈机”很重要？

表达式：

```python
z = a * b + c
```

在栈机中很自然：

```text
LOAD a
LOAD b
MUL
LOAD c
ADD
STORE z
```

每个操作从栈顶取值，再把结果压回栈。

### 视觉化

```text
开始: []
LOAD a -> [a]
LOAD b -> [a, b]
MUL    -> [a*b]
LOAD c -> [a*b, c]
ADD    -> [a*b+c]
STORE z
```

这种设计让编译器更简单。

---

# 7. Python 函数调用的底层图像

看：

```python
def add(x, y):
    return x + y

add(3, 4)
```

## 7.1 调用发生了什么

```text
调用点
   │
   ▼
创建新的 frame（栈帧）
   │
   ├── 局部变量槽位: x=3, y=4
   ├── 指向当前代码对象
   ├── 记录返回地址
   └── 自己的操作数栈
   │
   ▼
执行函数字节码
   │
   ▼
得到返回值
   │
   ▼
弹出 frame，回到调用者
```

## 7.2 为什么 frame 很关键

因为它把一次函数调用隔离成一个独立执行上下文：

- 当前函数自己的局部变量
- 自己的执行位置
- 自己的临时栈

这也是递归能工作的基础。

---

# 8. Python 中的模块导入原理

## 8.1 `import x` 到底做了什么

可以粗略理解为：

```text
1. 先看模块缓存 sys.modules 里有没有
2. 没有就去文件系统/路径查找
3. 找到源文件或扩展模块
4. 加载并执行模块初始化代码
5. 把模块对象放入 sys.modules
```

## 8.2 为什么要缓存

因为模块本质上也是对象，且初始化可能有副作用。

```python
import foo
import foo
```

不是执行两遍，而是复用同一个模块对象。

---

# 9. Python 的 GIL 是什么，为什么存在？

## 9.1 先说效果

GIL = **Global Interpreter Lock**，全局解释器锁。

在一个 CPython 解释器进程里：

- 多个线程可以存在
- 但同一时刻通常只有一个线程执行 Python 字节码

## 9.2 为什么会这样

因为 CPython 的很多内部数据结构并不是天生线程安全的，尤其：

- 引用计数加减
- 对象访问
- 容器状态修改

GIL 的工程意义是：

```text
“先用一把大锁，保护解释器核心一致性。”
```

这样实现简单很多。

## 9.3 代价是什么

CPU 密集型 Python 线程不能真正并行跑字节码。

## 9.4 为什么 pybind11 经常提到 GIL

因为一旦你从 Python 调 C++：

- 如果 C++ 只是算数值，可以释放 GIL，让别的 Python 线程继续跑。
- 如果 C++ 要创建/操作 Python 对象，就必须持有 GIL。

这是 pybind11 设计中的关键边界。

---

# 10. 如果你要“重新发明一个最小 Python”，思路是什么？

不要一上来想完整 Python。正确思路是按层搭积木。

## 10.1 第 1 阶段：先做表达式计算器

支持：

```text
1 + 2 * 3
```

你需要：

- tokenizer
- parser
- AST
- evaluator 或 bytecode VM

### 最小结构图

```text
文本 "1+2*3"
   ↓
Tokens [1, +, 2, *, 3]
   ↓
AST
   Add
  /   \
 1    Mul
     /   \
    2     3
   ↓
结果 7
```

## 10.2 第 2 阶段：加入变量

支持：

```python
x = 10
y = x + 2
```

你需要一个环境表：

```text
env = {
  "x": 10,
  "y": 12
}
```

## 10.3 第 3 阶段：加入对象系统

不要只存裸 `int`，而是存统一对象：

```text
Object*
  ├── type=int
  ├── type=str
  └── type=list
```

这样才能走向真正的动态语言。

## 10.4 第 4 阶段：加入函数与 frame

支持：

```python
def f(x):
    return x + 1
```

你需要：

- 代码对象
- 调用栈
- frame
- return 机制

## 10.5 第 5 阶段：加入 GC / 模块 / 异常

这时候才开始接近“语言运行时”而不是“脚本计算器”。

## 10.6 最小版重发明路线图

```text
计算器
  → 变量
  → 控制流
  → 函数
  → 对象系统
  → 内存管理
  → 模块系统
  → C API
```

这个顺序比“从语法细节入手”更接近真正实现者的思路。

---

# 11. 从 Python 切到 pybind11：它到底在解决什么痛点？

假设你有一个 C++ 函数：

```cpp
int add(int a, int b) {
    return a + b;
}
```

你想在 Python 里这样用：

```python
import mylib
mylib.add(1, 2)
```

## 11.1 不用 pybind11 时你要做什么

你要直接写 CPython C API：

1. 写模块初始化函数
2. 写 Python 可调用包装函数
3. 从 `PyObject* args` 解析参数
4. 调真实 C/C++ 函数
5. 把返回值重新包成 `PyObject*`
6. 处理异常、引用计数、错误码

这很繁琐，而且容易错。

## 11.2 pybind11 的目标

一句话：

```text
把“手写 CPython C API 胶水代码”这件事模板化、类型化、现代 C++ 化。
```

也就是：

- 你声明“把这个函数暴露出去”
- pybind11 帮你生成中间胶水层

---

# 12. pybind11 的最小例子

```cpp
#include <pybind11/pybind11.h>
namespace py = pybind11;

int add(int a, int b) {
    return a + b;
}

PYBIND11_MODULE(mylib, m) {
    m.def("add", &add);
}
```

然后 Python 里：

```python
import mylib
print(mylib.add(3, 4))
```

## 12.1 这段绑定代码在“概念上”干了什么

`m.def("add", &add)` 背后大致意味着：

```text
注册一个 Python 函数名 "add"
      │
      ▼
当 Python 调用它时
      │
      ├── 从 Python 参数对象里取出实参
      ├── 转成 C++ int, int
      ├── 调用真实 C++ 函数 add
      ├── 把返回的 int 转回 Python int 对象
      └── 返回给解释器
```

pybind11 的底层秘密基本都围绕这几步展开。

---

# 13. pybind11 的核心原理 1：它本质上是“胶水代码生成器”

虽然看起来你只是写了 `m.def(...)`，但实际效果像是自动生成了如下包装器：

```cpp
PyObject* wrapped_add(PyObject* self, PyObject* args) {
    int a = parse_int(args, 0);
    int b = parse_int(args, 1);
    int r = add(a, b);
    return make_pyint(r);
}
```

当然真正实现远比这复杂，要处理：

- 重载
- 默认参数
- 关键字参数
- 异常翻译
- 生命周期
- 自定义类
- numpy buffer
- 智能指针

但精神上就是：

**把 C++ 调用包装成 Python 能理解的 ABI 入口。**

---

# 14. pybind11 的核心原理 2：`PyObject*` 是边界，C++ 类型是内部世界

## 14.1 运行时边界图

```text
Python 调用侧
mylib.add(1, 2)
   │
   ▼
CPython 只认识 PyObject*
   │
   ▼
pybind11 wrapper
   │   ├── PyObject* -> int
   │   ├── 调 add(int, int)
   │   └── int -> PyObject*
   ▼
C++ 真正函数
```

关键理解：

- **CPython 世界的通用货币是 `PyObject*`**
- **C++ 世界的通用货币是静态类型**
- **pybind11 就是汇率兑换层**

---

# 15. pybind11 的核心原理 3：type caster 类型转换器

这是 pybind11 最关键的思想之一。

## 15.1 什么是 type caster

如果你有函数：

```cpp
std::string greet(std::string name)
```

Python 传入的是 Python `str` 对象，C++ 需要的是 `std::string`。

所以 pybind11 需要一套规则：

```text
Python str  <->  C++ std::string
Python int  <->  C++ int
Python list <->  C++ std::vector<T>
```

每种类型转换规则，本质就是一个 **type caster**。

## 15.2 视觉化

```text
PyObject*("hello")
      │
      ▼
 type_caster<std::string>::load(...)
      │
      ▼
std::string("hello")
```

返回值方向：

```text
std::string("hello")
      │
      ▼
 type_caster<std::string>::cast(...)
      │
      ▼
PyObject* (Python str)
```

## 15.3 这件事为什么强大

因为一旦定义了某个类型的 caster，所有使用这个类型的函数都能自动受益。

这就是 pybind11 的可扩展性来源之一。

---

# 16. pybind11 的核心原理 4：函数绑定其实是模板元编程 + 运行时注册

看：

```cpp
m.def("add", &add);
```

这里编译器已经知道 `&add` 的类型是：

```cpp
int (*)(int, int)
```

所以 pybind11 能在编译期推断出：

- 返回类型 `int`
- 参数类型 `(int, int)`

然后生成对应包装逻辑。

## 16.1 编译期做什么

```text
分析函数签名
  ↓
生成“如何逐个参数转换”的代码模板
  ↓
生成“如何把返回值转回 Python”的代码模板
```

## 16.2 运行时做什么

```text
把这个包装函数注册到模块对象里
  ↓
Python 运行时看到它是一个可调用对象
  ↓
用户调用时，实际进入包装函数
```

所以 pybind11 是：

- **编译期**：推类型、生成包装逻辑
- **运行时**：向 CPython 注册对象和调用入口

---

# 17. pybind11 绑定类的原理

看一个类：

```cpp
class Pet {
public:
    Pet(std::string name) : name_(name) {}
    std::string name() const { return name_; }
private:
    std::string name_;
};
```

绑定：

```cpp
py::class_<Pet>(m, "Pet")
    .def(py::init<std::string>())
    .def("name", &Pet::name);
```

## 17.1 背后在做什么

pybind11 需要做三件关键事：

### 1) 创建一个 Python 类型对象

```text
Python 侧出现一个新类型: mylib.Pet
```

### 2) 定义这个类型的构造、析构、方法表

```text
Pet("Milo") -> 调 C++ 构造函数
obj.name()  -> 调 C++ 成员函数
```

### 3) 在 Python 对象里保存“指向 C++ 实例的指针”

概念图：

```text
Python Pet object
├── PyObject 头
└── holder / pointer
    └── C++ Pet instance
```

所以 Python 对象本身通常不是完整 C++ 对象，而是一个**包装壳**。

---

# 18. 生命周期：为什么 pybind11 比“能调用”更难的是“谁拥有对象”

这是绑定系统里最容易踩坑的地方。

## 18.1 三种常见所有权问题

### 场景 A：Python 创建，Python 销毁

```python
p = mylib.Pet("Milo")
```

这时通常：

- Python 包装对象拥有 C++ 对象
- Python 对象销毁时，C++ 对象也删掉

### 场景 B：C++ 返回裸指针

```cpp
Pet* get_pet();
```

这时问题是：

- 返回给 Python 后谁负责 `delete`？
- 这个指针是不是还被 C++ 别处持有？

### 场景 C：共享所有权

```cpp
std::shared_ptr<Pet>
```

这时 pybind11 可以借助智能指针把所有权语义表达清楚。

## 18.2 可视化

### 裸指针危险版

```text
Python wrapper ───► Pet* raw
C++ somewhere ───► same Pet
```

如果一边删了，另一边就悬空。

### shared_ptr 稳定版

```text
Python wrapper ─┐
                ├──► shared_ptr control block ───► Pet
C++ owner    ───┘
```

## 18.3 为什么 pybind11 提供 return value policy

就是为了声明：

- 返回值是复制？
- 借用？
- 转移所有权？
- 引用内部对象？

这不是语法装饰，而是绑定系统的生死线。

如果这一层说错了，最常见的后果就是：

- Python 提前把 C++ 对象删掉
- C++ 还活着，但 Python 拿到悬空引用
- 明明只是借看一下，结果被当成拥有者去释放
- 明明应该跟宿主对象同生共死，却被 Python 单独长期持有

所以 return value policy 本质上是在回答一个问题：

```text
Python 拿到这个返回值后，到底“拥有”它，还是只是“临时看见”它？
```

---

### 18.3.1 先用一句话记住每种 policy

先记最常见的几个：

- `automatic`：让 pybind11 按返回类型猜
- `take_ownership`：Python 接手对象所有权
- `copy`：复制一个新对象给 Python
- `move`：把对象移动给 Python
- `reference`：只是借一个引用，不负责销毁
- `reference_internal`：借引用，但生命周期跟宿主对象绑定

可以先把它们想成：

```text
copy / move            -> 给 Python 一个“独立可拥有”的结果
reference              -> 给 Python 一个“别乱删”的借条
reference_internal     -> 给 Python 一个“挂靠父对象”的借条
```

---

### 18.3.2 最容易犯错的场景：返回裸指针

看这个例子：

```cpp
class Pet {
public:
    std::string name;
};

Pet* make_pet() {
    return new Pet{"Milo"};
}
```

如果这是“工厂函数”，它新建对象就是要交给外部，那通常语义是：

```cpp
m.def("make_pet", &make_pet, py::return_value_policy::take_ownership);
```

#### 为什么

因为这相当于在说：

```text
这个 Pet* 没有别的主人了
Python wrapper 现在接手 delete 责任
```

#### 视觉化

```text
C++ new Pet
   │
返回 Pet*
   │
   ▼
Python wrapper 拥有它
   │
Python 对象析构时 delete Pet
```

#### 最佳实践

- 如果函数返回 `new` 出来的裸指针，并且语义上“调用者负责释放”，优先明确写 `take_ownership`
- 更推荐直接返回 `std::unique_ptr<Pet>` 或 `std::shared_ptr<Pet>`，比裸指针更不容易犯错

---

### 18.3.3 返回内部成员引用：最该想到 `reference_internal`

看：

```cpp
class Zoo {
public:
    Pet& pet() { return pet_; }
private:
    Pet pet_;
};
```

这里 `pet()` 返回的不是独立对象，而是 `Zoo` 内部成员。

这时最典型写法是：

```cpp
py::class_<Zoo>(m, "Zoo")
    .def(py::init<>())
    .def("pet", &Zoo::pet, py::return_value_policy::reference_internal);
```

#### 为什么不能乱用 `take_ownership`

因为 Python 并不真的拥有这个 `Pet`，它只是看到 `Zoo` 体内那一块对象。

如果错误地让 Python 以为自己拥有它，结果可能是：

- Python 尝试单独销毁这个内部成员
- 或 `Zoo` 已经销毁后，Python 还拿着悬空引用

#### `reference_internal` 的直觉

```text
返回的是“子对象视图”
Python 可以拿着它
但必须保证父对象 Zoo 活着
```

#### 视觉化

```text
Python Zoo wrapper ─────► Zoo instance
                            └── pet_
                                ▲
                                │
                    Python Pet view (non-owning)
```

#### 最佳实践

- 返回成员引用 / 成员指针 / 容器内部元素视图时，优先考虑 `reference_internal`
- 记忆句：**返回值依附于 `self` 内部存活时，用 `reference_internal`**

---

### 18.3.4 返回全局单例或长期存在对象：常见是 `reference`

看：

```cpp
Pet global_pet{"Boss"};

Pet& get_global_pet() {
    return global_pet;
}
```

这里对象不是新建给 Python 的，也不是某个临时局部对象，而是长期存在的全局对象。

这时可以表达成：

```cpp
m.def("get_global_pet", &get_global_pet, py::return_value_policy::reference);
```

#### 为什么

因为 Python 只是借到一个引用：

- 不负责 delete
- 不应该假装拥有它

#### 最佳实践

- 返回全局对象、单例对象、由别处长期托管的对象时，用 `reference`
- 前提是你真的能保证这个对象在 Python 使用期内一直活着

如果活不够久，`reference` 一样会出问题。

---

### 18.3.5 返回值语义对象：优先让 copy / move 自然发生

看：

```cpp
struct Point {
    int x;
    int y;
};

Point make_point() {
    return Point{1, 2};
}
```

这种返回值本身就是值语义，通常最自然。

```cpp
m.def("make_point", &make_point);
```

或显式说明：

```cpp
m.def("make_point", &make_point, py::return_value_policy::move);
```

#### 直觉

- 返回的是一个独立结果
- 不依赖外部宿主对象
- 给 Python 一个自己的副本/移动结果最安全

#### 最佳实践

- 如果函数返回普通值对象，通常不必过度干预，默认策略往往足够
- 对大型值对象，如果你明确知道 move 语义更合适，可以显式写 `move`
- 对小而便宜的对象，`copy` / 默认通常就很好理解

---

### 18.3.6 一个“不要这样做”的危险例子：返回局部变量引用

```cpp
const std::string& bad_name() {
    std::string s = "tmp";
    return s;
}
```

这是 C++ 层面本来就错的，pybind11 也救不了。

因为：

```text
函数一返回
局部变量就销毁
Python 拿到的一定是悬空引用
```

#### 最佳实践

- 不要指望 return value policy 修复本来就非法的 C++ 生命周期
- 先保证 C++ API 本身正确，再谈绑定策略

这个判断非常重要：

**policy 负责表达所有权，不负责拯救失效对象。**

---

### 18.3.7 最实用的选择口诀

你可以先按这个顺序判断：

#### 情况 A：这是新创建出来、就是交给调用方管理的吗？

- 是 -> `take_ownership`
- 更优方案 -> 直接返回 `unique_ptr` / `shared_ptr`

#### 情况 B：这是一个独立值对象吗？

- 是 -> 默认 / `copy` / `move`

#### 情况 C：这是借来的对象，而且它独立长期存在吗？

- 是 -> `reference`

#### 情况 D：这是宿主对象内部的一部分吗？

- 是 -> `reference_internal`

压成图：

```text
new 出来给你      -> take_ownership
独立值结果         -> copy / move / automatic
借全局/单例        -> reference
借 self 内部成员    -> reference_internal
```

---

### 18.3.8 pybind11 绑定中的最佳实践

#### 最佳实践 1：能不用裸指针，就不用裸指针

优先：

```cpp
std::unique_ptr<T>
std::shared_ptr<T>
T
```

再考虑：

```cpp
T*
T&
```

因为裸指针会把所有权语义隐藏起来，让绑定层必须猜。

---

#### 最佳实践 2：返回“内部成员”时，默认先怀疑要用 `reference_internal`

尤其是这类接口：

```cpp
T& obj.child();
T* obj.data();
const U& obj.value() const;
```

先问自己：

```text
这个返回值是不是依赖 obj 活着？
```

如果是，`reference_internal` 往往比 `reference` 更安全。

---

#### 最佳实践 3：如果 API 本意是共享所有权，就在 C++ 层直接表达出来

比如：

```cpp
std::shared_ptr<Pet> get_pet();
```

比：

```cpp
Pet* get_pet();
```

更清晰。

这样 pybind11 更容易正确传播生命周期。

---

#### 最佳实践 4：把“谁 delete”说清楚，再写绑定

写绑定前先回答：

- 这个对象是谁创建的？
- 谁应该销毁它？
- 返回值是否依赖某个父对象存在？
- Python 是否应该长期持有它？

如果这几个问题在 C++ API 设计里本身就模糊，绑定层大概率也会模糊。

---

#### 最佳实践 5：面向教学时，优先选择“最不惊讶”的语义

如果你在做 tutorial / demo：

- 工厂函数返回值对象或智能指针
- 成员访问器返回 `reference_internal`
- 避免让初学者一上来就面对裸指针 ownership puzzle

这样用户最容易形成稳定心智模型。

---

### 18.3.9 一张总结表

| C++ 返回形式 | 常见语义 | 常见 policy / 做法 |
|---|---|---|
| `T` | 独立值结果 | 默认 / `copy` / `move` |
| `std::unique_ptr<T>` | 转移唯一所有权 | 直接返回智能指针 |
| `std::shared_ptr<T>` | 共享所有权 | 直接返回智能指针 |
| `T*` 指向 `new` 对象 | Python 接管释放 | `take_ownership` |
| `T&` / `T*` 指向全局或单例 | 借用，不释放 | `reference` |
| `T&` / `T*` 指向 `self` 内部成员 | 借用且依赖父对象 | `reference_internal` |

一句话压缩：

**先判断对象真正归谁，再选择 policy；如果 C++ 类型已经能表达所有权，就尽量让类型系统说话，而不是让 policy 去猜。**

---

# 19. pybind11 与异常：为什么 C++ throw 能变成 Python exception

如果 C++ 函数：

```cpp
int divide(int a, int b) {
    if (b == 0) throw std::runtime_error("divide by zero");
    return a / b;
}
```

Python 调用时我们希望：

```python
mylib.divide(1, 0)
# -> 抛 Python 异常，而不是进程崩掉
```

## 19.1 pybind11 的思路

包装函数类似：

```cpp
try {
    // 参数转换 + 调 C++
} catch (const std::exception& e) {
    // 设置 Python 异常状态
    return error;
}
```

所以 pybind11 做了“异常世界翻译”：

```text
C++ exception  ->  Python exception state
```

这也是胶水层的重要职责。

---

# 20. pybind11 与继承 / trampoline

这一节建议你把 **trampoline** 先记成一句话：

```text
trampoline = 一个 C++ 中间子类
它 override 虚函数
并把 C++ 的虚调用转发到 Python override
```

名字很形象，像“跳板”：

```text
C++ virtual call
      ↓
trampoline class
      ↓
Python override
```

它解决的不是普通函数绑定，而是更难的一类问题：

```text
C++ 用 virtual 做动态分派
Python 用方法重写做动态分派
pybind11 要把这两套分派机制接起来
```

---

## 20.1 先看目标：我们到底想实现什么效果

C++ 基类：

```cpp
class Animal {
public:
    virtual std::string sound() const {
        return "...";
    }
    virtual ~Animal() = default;
};
```

Python 子类：

```python
class Dog(mylib.Animal):
    def sound(self):
        return "woof"
```

我们希望不只是：

```python
d = Dog()
d.sound()
```

能返回 `woof`。

更重要的是，希望 **C++ 里通过基类引用发起的虚调用** 也能走到 Python：

```cpp
std::string describe(const Animal& a) {
    return a.sound();
}
```

然后 Python 里：

```python
d = Dog()
mylib.describe(d)   # 希望结果是 "woof"
```

这才是真正的难点。

---

## 20.2 难点到底在哪

C++ 的虚函数调用只认：

- 对象的 C++ 动态类型
- 对应的 vtable

它并不知道 Python 类里有没有写：

```python
def sound(self): ...
```

所以如果没有额外桥接，C++ 只会看到：

```text
Animal&
```

而不会自动说：

```text
“哦，这个对象背后其实是 Python 子类，去 Python 里找 override 吧。”
```

这就是为什么普通绑定不够，必须有 trampoline。

---

## 20.3 trampoline 的核心机制

trampoline 本质上是一个 **真正的 C++ 子类**。

它一边对 C++ 来说合法参与虚函数分派，一边又会在 override 里回头查 Python 是否重写了方法。

调用链图：

```text
C++ 调 a.sound()
   │
   ▼
进入 trampoline::sound()
   │
   ▼
检查 Python 子类是否重写了 sound
   │
   ├── 有：调用 Python sound(self)
   └── 没有：回退到 C++ 基类默认实现
```

所以 trampoline 不是“额外语法糖”，而是：

**把 C++ 虚分派桥接到 Python 动态方法分派的适配层。**

---

## 20.4 第一个代码点：普通 C++ 基类

先把“被继承、被 override 的那一侧”看清楚。

```cpp
class Animal {
public:
    Animal() = default;
    virtual ~Animal() = default;

    virtual std::string sound() const {
        return "...";
    }
};
```

这个类本身完全不知道 Python。

它只是一个普通的 C++ 多态基类。

---

## 20.5 第二个代码点：trampoline class 长什么样

```cpp
class PyAnimal : public Animal {
public:
    using Animal::Animal;

    std::string sound() const override {
        PYBIND11_OVERRIDE(
            std::string,
            Animal,
            sound
        );
    }
};
```

这一小段就是 trampoline 的核心。

逐行理解：

### `class PyAnimal : public Animal`

说明它是一个真正的 C++ 子类，能进入 C++ 的虚函数系统。

### `using Animal::Animal;`

把基类构造函数继承下来，方便 pybind11 构造对象。

### `sound() const override`

说明它真的 override 了虚函数。

### `PYBIND11_OVERRIDE(...)`

这不是普通函数体，而是一段“去 Python 查 override 并调用”的模板逻辑。

你可以把它先粗略想成：

```cpp
std::string sound() const override {
    // 1. 找到关联的 Python 对象
    // 2. 看 Python 是否实现了 sound
    // 3. 如果实现了，就调用 Python override
    // 4. 把返回值转成 std::string
    // 5. 否则回退到基类默认实现
}
```

---

## 20.6 第三个代码点：绑定时为什么要写 `class_<Animal, PyAnimal>`

```cpp
py::class_<Animal, PyAnimal>(m, "Animal")
    .def(py::init<>())
    .def("sound", &Animal::sound);
```

这里第二个模板参数 `PyAnimal` 极关键。

它的意思不是“额外再绑定一个类”，而是：

```text
当 Python 侧参与继承和虚调用桥接时
这个绑定类型要使用 PyAnimal 作为 trampoline
```

如果你只写：

```cpp
py::class_<Animal>(m, "Animal")
```

那么 Python 虽然可以看到 `Animal`，但 C++ 的虚调用链就没有这个“跳板子类”，很多 override 场景就桥不上。

---

## 20.7 第四个代码点：真正证明 trampoline 生效的 C++ 调用函数

很多人误以为下面这种就已经证明成功：

```python
d = Dog()
d.sound()
```

其实这只能证明 Python 子类方法本身能调。

真正要验证 trampoline，应该写一个 **C++ 里发起虚调用** 的函数：

```cpp
std::string describe(const Animal& animal) {
    return "C++ heard: " + animal.sound();
}
```

然后 Python 里：

```python
d = Dog()
print(mylib.describe(d))
```

如果输出是：

```text
C++ heard: woof
```

这才说明：

```text
C++ 的 virtual dispatch
真的经由 trampoline
回调到了 Python override
```

---

## 20.8 第五个代码点：Python 侧 override 长什么样

```python
class Dog(mylib.Animal):
    def sound(self):
        return "woof"
```

这里在 Python 眼里只是普通继承。

但底层更像：

```text
Python Dog instance
   │
   ▼
关联到一个 C++ 侧可参与虚分派的包装对象
   │
   ▼
当 C++ 调 virtual sound()
   │
   ▼
会先进入 PyAnimal::sound()
   │
   ▼
再回调 Dog.sound(self)
```

也就是说，Python 用户看到的是自然语法，桥接复杂性被 trampoline 隐藏了。

---

## 20.9 一个完整调用链图

把上面几段代码串起来：

```text
Python:
class Dog(Animal):
    def sound(self):
        return "woof"

Python 调 mylib.describe(d)
   │
   ▼
进入 C++ describe(const Animal&)
   │
   ▼
describe 内部调用 animal.sound()
   │
   ▼
命中 trampoline: PyAnimal::sound()
   │
   ▼
trampoline 去查 Python override
   │
   ▼
调用 Dog.sound(self)
   │
   ▼
返回 "woof"
   │
   ▼
回到 C++ describe
   │
   ▼
回到 Python
```

这就是 trampoline 的本质价值。

---

## 20.10 `PYBIND11_OVERRIDE` 和 `PYBIND11_OVERRIDE_PURE` 的区别

### 情况 A：C++ 基类有默认实现

```cpp
class Animal {
public:
    virtual std::string sound() const {
        return "...";
    }
};
```

对应 trampoline：

```cpp
std::string sound() const override {
    PYBIND11_OVERRIDE(
        std::string,
        Animal,
        sound
    );
}
```

意思是：

- Python 有 override -> 调 Python
- Python 没 override -> 允许回退到 C++ 默认实现

---

### 情况 B：C++ 基类是纯虚函数

```cpp
class Animal {
public:
    virtual std::string sound() const = 0;
    virtual ~Animal() = default;
};
```

对应 trampoline：

```cpp
std::string sound() const override {
    PYBIND11_OVERRIDE_PURE(
        std::string,
        Animal,
        sound
    );
}
```

意思是：

- Python 必须实现 `sound`
- 如果没实现，这是错误，不允许回退

### 记忆法

```text
有默认实现  -> OVERRIDE
纯虚函数    -> OVERRIDE_PURE
```

---

## 20.11 trampoline 和普通绑定的本质区别

### 普通函数绑定

```cpp
m.def("add", &add);
```

解决的是：

```text
Python 调 C++ 函数
```

### trampoline 绑定

```cpp
py::class_<Animal, PyAnimal>(m, "Animal")
```

解决的是：

```text
C++ 发起虚调用
↓
Python override 接管行为
```

所以 trampoline 处理的是一个更深层的问题：

**不是单次调用跨语言，而是“多态分派跨语言”。**

---

## 20.12 为什么 trampoline 一定要是“中间子类”

很多人会问：

> 为什么不直接让 pybind11 在 `Animal` 上做点魔法就行？

关键原因是：

```text
C++ virtual dispatch 依赖对象的动态类型和 vtable
```

而 trampoline 恰好提供了：

- 一个真实的 C++ 子类类型
- 一个真实的 override 实现
- 一个在 override 中回调 Python 的入口

所以它必须长成“子类”的样子，而不是简单函数包装器。

---

## 20.13 trampoline 和 GIL 的关系

只要 trampoline 要回调 Python，它就一定会碰 Python 运行时。

所以底层必然涉及：

- 获取 / 确保持有 GIL
- 查找 Python override
- 调 Python 方法
- 把结果转换回 C++

也就是说：

```text
trampoline = 虚函数桥接 + Python 回调
```

而 **Python 回调** 这件事本身就离不开 GIL。

所以 trampoline 不只是“虚函数技巧”，它也是跨语言 runtime 边界的一部分。

---

## 20.14 最小完整示例：按知识点拆开的版本

### 1) C++ 基类

```cpp
class Animal {
public:
    virtual std::string sound() const {
        return "...";
    }
    virtual ~Animal() = default;
};
```

### 2) trampoline 类

```cpp
class PyAnimal : public Animal {
public:
    using Animal::Animal;

    std::string sound() const override {
        PYBIND11_OVERRIDE(
            std::string,
            Animal,
            sound
        );
    }
};
```

### 3) C++ 侧虚调用函数

```cpp
std::string describe(const Animal& animal) {
    return animal.sound();
}
```

### 4) pybind11 绑定

```cpp
py::class_<Animal, PyAnimal>(m, "Animal")
    .def(py::init<>())
    .def("sound", &Animal::sound);

m.def("describe", &describe);
```

### 5) Python 子类

```python
class Dog(mylib.Animal):
    def sound(self):
        return "woof"
```

### 6) Python 验证调用

```python
d = Dog()
print(d.sound())         # woof
print(mylib.describe(d)) # woof
```

第二行才是 trampoline 真正工作的证据。

---

## 20.15 常见误区

### 误区 1：`d.sound()` 能调通，就说明 trampoline 没问题

不一定。

```python
d.sound()
```

只能证明 Python 子类自己的方法能运行。

真正要测试的是：

```python
mylib.describe(d)
```

因为这才会强迫调用链走进 C++ 的虚函数分派。

---

### 误区 2：绑定类时忘了 trampoline 模板参数

错误示意：

```cpp
py::class_<Animal>(m, "Animal")
```

正确示意：

```cpp
py::class_<Animal, PyAnimal>(m, "Animal")
```

第二个模板参数就是桥。

---

### 误区 3：纯虚函数却用了 `PYBIND11_OVERRIDE`

如果 C++ 根本没有默认实现，却还写普通 `OVERRIDE`，语义就不清晰。

更稳妥的写法是：

```cpp
PYBIND11_OVERRIDE_PURE(...)
```

这样更符合“必须由子类实现”的契约。

---

### 误区 4：把 trampoline 想成“普通包装类”

它不是简单包一层 API，而是要真的进入 C++ 多态系统。

所以它本质是：

```text
能参与 vtable 分派的 C++ 子类
+ 能回调 Python 的 override 实现
```

---

## 20.16 对照你当前示例工程里的文件

如果你已经看过当前目录下的示例代码，这几部分可以这样对应：

### `src/animal.h`

```text
普通 C++ 基类
```

### `src/py_animal_trampoline.h`

```text
trampoline class
这里是“C++ virtual -> Python override”的核心桥
```

### `src/describe.cpp`

```text
C++ 发起虚调用的地方
用于证明 trampoline 真的生效
```

### `src/bindings.cpp`

```text
把 Animal 和 PyAnimal 绑定到 pybind11
关键写法是 class_<Animal, PyAnimal>
```

### `demo.py`

```text
Python 里继承 Animal
并用 describe(...) 验证 C++ 虚调用是否回到了 Python
```

这样分文件看，会比把所有代码塞在一个文件里更容易理解调用链。

---

## 20.17 一句话总结

```text
trampoline 不是为了“让 Python 能继承 C++”这么简单，
而是为了让 C++ 的 virtual dispatch 真正跳转到 Python override。
```

再压缩一点：

```text
没有 trampoline：C++ 只能看到 C++ vtable
有了 trampoline：C++ virtual call 才能桥接到 Python 方法重写
```

---

# 21. pybind11 与 GIL：什么时候该拿锁，什么时候该放锁

这是 pybind11 里最容易“表面会用，底层其实没吃透”的点之一。

先记一句总原则：

```text
只要你在碰 Python 运行时，就必须持有 GIL；
只有当你暂时完全离开 Python 世界、只做纯 C++ 工作时，才适合释放 GIL。
```

这里的“碰 Python 运行时”不只是“写 Python 代码”，还包括：

- 创建 Python 对象
- 访问 `py::object` / `py::list` / `py::dict`
- 递增/递减 Python 对象引用计数
- 抛出/设置 Python 异常
- 调 Python 回调
- 调用任何依赖 CPython 内部状态的 API

也就是说，在 pybind11 语境下，**GIL 保护的不只是字节码执行，而是整个 CPython 对象世界的一致性。**

---

## 21.1 先建立一个最稳的边界图

```text
Python caller
   │
   ▼
进入 pybind11 包装层
   │
   ├── 参数解析（需要 Python 对象）   -> 必须有 GIL
   ├── 调纯 C++ 计算内核            -> 可以考虑释放 GIL
   ├── 包装返回值为 Python 对象      -> 必须有 GIL
   └── 如果中途回调 Python          -> 必须重新拿回 GIL
```

所以你可以把 GIL 理解成一张“跨边界通行证”：

- 在 Python 边界内活动：必须持证
- 进入纯 C++ 区：可以临时把证放下
- 回 Python 边界：必须再把证拿回来

---

## 21.2 规则图

```text
要碰 Python 对象？              -> 必须持有 GIL
纯 C++ 长时间计算？             -> 可以释放 GIL
从 C++ 回调 Python？            -> 必须重新获取 GIL
在后台线程里创建 py::object？    -> 必须先 acquire GIL
销毁持有 Python 引用的对象？      -> 析构时也可能需要 GIL
```

最后一条很多人会漏掉：

**不仅“用 Python 对象”需要 GIL，很多时候“释放 Python 对象”也需要 GIL。**

因为析构一个 `py::object` 往往意味着要改引用计数，而改引用计数就是在碰 CPython 运行时。

---

## 21.3 为什么 CPython 需要 GIL

先别急着把 GIL 看成“性能坏东西”，先看它解决的问题。

CPython 里大量核心状态并不是细粒度线程安全的，尤其：

- 引用计数增减
- 对象分配/释放
- 容器内部状态修改
- 异常状态
- 解释器全局/线程局部状态

GIL 的工程意义就是：

```text
先用一把全局锁，把解释器对象世界保护起来
让 C 扩展和运行时实现大幅简化
```

而 pybind11 作为 C++ <-> Python 边界层，必须严格遵守这套规则。

---

## 21.4 一个最容易记住的直觉：pybind11 函数入口默认已经拿着 GIL

比如你绑定：

```cpp
m.def("add", [](int a, int b) {
    return a + b;
});
```

当 Python 调用它时：

```python
m.add(1, 2)
```

进入这个 lambda 时，一般已经处在“由 Python 调过来”的上下文中，所以默认已经持有 GIL。

也就是说：

- 参数转换发生时有 GIL
- 你的绑定函数开始执行时有 GIL
- 返回值转回 Python 对象时有 GIL

所以通常不是“怎么拿到 GIL”的问题，而是：

```text
我是不是应该在某段纯 C++ 工作期间把它主动释放掉？
```

---

## 21.5 什么时候适合释放 GIL

最典型场景：

- 长时间纯数值计算
- 图像处理内核
- 大规模循环但完全不碰 Python 对象
- 阻塞式 I/O（前提是 I/O 期间不操作 Python 对象）
- 调用线程安全的 C/C++ 库并等待其完成

### 例子 1：纯数值计算

```cpp
double heavy_compute(int n) {
    double s = 0;
    for (int i = 0; i < n; ++i) {
        s += i * 0.1;
    }
    return s;
}
```

绑定时可以这样：

```cpp
m.def("heavy_compute", &heavy_compute, py::call_guard<py::gil_scoped_release>());
```

### 效果

Python 调这个函数时：

- 进入包装层先拿着 GIL
- 调 `heavy_compute` 前临时释放 GIL
- 结束后自动重新获取 GIL
- 再把返回值包装回 Python

这让其他 Python 线程在这段时间里也能继续执行。

---

## 21.6 什么时候绝对不该释放 GIL

只要你的函数内部做了下面任意事情，就不能“整段无脑释放”：

- 创建 `py::object`
- 操作 `py::list` / `py::dict`
- 访问 Python 属性
- 调 Python 函数 / Python 回调
- 返回过程中依赖 Python 对象生命周期
- 操作任何需要 CPython API 的代码

### 例子 2：构造 Python list

```cpp
py::list build_list() {
    py::list out;
    out.append(1);
    out.append(2);
    return out;
}
```

这显然不适合在函数主体期间释放 GIL，因为整个函数几乎都在碰 Python 对象。

### 例子 3：回调 Python

```cpp
void call_twice(py::function f) {
    f();
    f();
}
```

`f()` 本身就是 Python 调用，必须持有 GIL。

---

## 21.7 最常见的正确模式：只在“纯 C++ 热区”局部释放 GIL

很多真实函数不是“全 Python”或“全 C++”，而是混合的。

例如：

```cpp
m.def("process", [](py::array_t<double, py::array::c_style> arr) {
    auto view = arr.mutable_unchecked<1>();

    {
        py::gil_scoped_release release;
        for (py::ssize_t i = 0; i < view.shape(0); ++i) {
            view(i) = view(i) * 2.0 + 1.0;
        }
    }

    return arr;
});
```

### 这段代码为什么合理

- 进入函数时，参数解析已经完成
- `view` 已经拿到对底层内存的访问句柄
- 释放 GIL 后只做纯 C++ 数值循环
- 离开作用域自动重新 acquire
- 返回 `arr` 给 Python 时重新处于安全状态

### 这个例子的隐含前提

- 循环体内部不能去碰 Python 对象
- 你对这块底层内存的并发访问语义要有把握
- 如果其他线程也可能同时改同一块数组，GIL 释放后不再替你提供数据竞争保护

这点非常重要：

**GIL 保护的是 Python 运行时，不是你自己的 C++ 数据结构。**

---

## 21.8 `py::gil_scoped_release` 和 `py::gil_scoped_acquire` 是什么

它们本质上是 RAII 守卫对象。

### `py::gil_scoped_release`

作用：

- 当前线程原本持有 GIL
- 构造对象时释放 GIL
- 离开作用域时自动重新获取 GIL

### `py::gil_scoped_acquire`

作用：

- 当前线程可能没有 GIL
- 构造对象时获取 GIL
- 离开作用域时自动恢复

这和 C++ 里的 `std::lock_guard` / `unique_lock` 很像，只不过锁对象变成了 GIL。

---

## 21.9 `py::call_guard<py::gil_scoped_release>()` 和手写局部 release 怎么选

### 方案 A：整个绑定调用都适合释放

```cpp
m.def("heavy_compute", &heavy_compute, py::call_guard<py::gil_scoped_release>());
```

适合：

- 函数核心逻辑几乎全是纯 C++
- 中间不会碰 Python 对象
- 没有 Python 回调

### 方案 B：只释放某一小段热区

```cpp
m.def("mixed", [](py::object obj) {
    // 先做一些 Python 相关工作

    {
        py::gil_scoped_release release;
        // 再跑纯 C++ 热循环
    }

    // 再继续处理 Python 返回值
});
```

适合：

- 只有中间一段真正值得释放 GIL
- 前后都要碰 Python 运行时

### 最佳实践

- 整个函数都纯 C++：优先 `call_guard`
- 混合函数：优先局部 `gil_scoped_release`

---

## 21.10 后台线程是 GIL 问题的高发区

如果你的 C++ 代码自己开线程：

```cpp
std::thread worker([] {
    // ...
});
```

那么这个线程不是从 Python 直接调用栈里长出来的“自动带 GIL 线程”。

如果它要碰 Python，就必须显式 acquire：

```cpp
std::thread worker([] {
    py::gil_scoped_acquire acquire;
    py::print("hello from worker");
});
```

### 为什么

因为这个线程进入 CPython 世界之前，没有合法持证。

### 最佳实践

- 后台线程默认当自己“没有 GIL”
- 只在必须碰 Python 的最小作用域 acquire
- acquire 后尽快 release，不要在后台线程里长时间占着 GIL

---

## 21.11 一个典型正确例子：后台线程做纯 C++，汇报结果时短暂拿 GIL

```cpp
void run_async(py::function callback) {
    std::thread([callback]() {
        int result = 0;
        for (int i = 0; i < 1000000; ++i) {
            result += i;
        }

        py::gil_scoped_acquire acquire;
        callback(result);
    }).detach();
}
```

### 这个设计的关键点

- 长时间计算阶段不占 GIL
- 只有在回调 Python 时短暂 acquire

### 但这里还要再提醒一个生命周期问题

`callback` 是 `py::function`，它内部持有 Python 引用。

所以要小心：

- 捕获/移动它时机是否安全
- 销毁它时是否持有 GIL

实际工程里更稳妥的做法，通常是：

- 明确线程边界
- 明确 callback 存活时间
- 尽量让 `py::object` 的创建和销毁都发生在已知持有 GIL 的区域

---

## 21.12 一个容易忽略的坑：析构函数里碰 Python

看一个危险模式：

```cpp
struct Holder {
    py::object obj;
    ~Holder() {
        // obj 析构时会 decref
    }
};
```

如果 `Holder` 在某个没有 GIL 的线程里析构，那么 `obj` 的析构可能就会在没有 GIL 的情况下改 Python 引用计数。

这非常危险。

### 最佳实践

- 持有 `py::object` 的 C++ 类型，要特别关注它在哪个线程析构
- 如果必须在不确定线程析构，考虑显式设计销毁路径
- 不要在任意后台线程里随意让大量 `py::object` 自然析构

一句话：

**Python 引用的生命周期管理，不只是“构造时”要想 GIL，“析构时”也要想。**

---

## 21.13 回调 Python 时的典型正确模式

例如你先释放 GIL 做重计算，结束后再回调 Python：

```cpp
m.def("run_and_callback", [](py::function cb, int n) {
    double result;

    {
        py::gil_scoped_release release;
        result = heavy_compute(n);
    }

    cb(result);
});
```

这段之所以成立，是因为：

- release 作用域结束后，GIL 已自动重新获取
- 所以 `cb(result)` 时又回到了安全区

### 错误思路

在 release 作用域里面直接：

```cpp
cb(result);
```

这是不行的，因为那时你并没有 GIL。

---

## 21.14 GIL 与死锁：为什么“先拿你自己的锁，再回调 Python”很危险

看这个模式：

```cpp
std::mutex mu;

m.def("bad", [&](py::function cb) {
    std::lock_guard<std::mutex> lock(mu);
    cb();
});
```

表面上你有 GIL，又拿了 `mu`，然后回调 Python。

危险在于：

- Python 回调里可能又触发别的路径
- 别的路径可能尝试获取 `mu`
- 或别的线程先拿了 `mu`，又在等 GIL

于是很容易形成锁顺序反转。

### 最佳实践

- 不要长时间同时持有“你自己的互斥锁 + GIL”
- 尤其不要在持有 C++ 锁时回调 Python
- 如果必须回调 Python，尽量先释放你自己的业务锁

GIL 只是锁的一种，也会参与死锁图。

---

## 21.15 GIL 与 NumPy / buffer：释放 GIL 不等于线程安全

比如你拿到一个 NumPy 数组 view：

```cpp
auto v = arr.mutable_unchecked<1>();
```

然后释放 GIL 做循环：

```cpp
{
    py::gil_scoped_release release;
    for (...) {
        v(i) += 1;
    }
}
```

这里要区分两件事：

### 你是“合法地离开 Python 运行时”了

对，没问题。

### 但这块底层内存是否会被并发访问？

GIL 不再保证。

如果别的线程也在：

- 改这块数组
- 改这块共享 C++ buffer
- 改与你算法相关的其他共享状态

那你仍然可能有 data race。

### 最佳实践

- GIL 只负责 Python runtime safety
- 共享数据一致性仍然要靠你自己的并发设计
- 不要把“我释放了 GIL”误解成“现在自动并行且安全”

---

## 21.16 最实用的判断清单

在释放 GIL 前，先问自己 5 个问题：

1. 这段代码里会不会创建、销毁、访问任何 `py::object`？  
2. 这段代码里会不会调用 Python 回调？  
3. 这段代码里会不会触发依赖 CPython API 的异常或对象操作？  
4. 我释放 GIL 后，底层共享数据会不会有并发竞争？  
5. 这个作用域结束时，是否能安全回到 Python 世界？  

只要前 3 个里有一个答案是“会”，就不应该无脑 release 整段。

---

## 21.17 最佳实践总结

### 最佳实践 1：默认保守，只有在明确纯 C++ 热区才释放 GIL

不要为了“听说这样更快”就到处加 `gil_scoped_release`。

---

### 最佳实践 2：用最小作用域释放 GIL

比起大范围 release，通常更推荐：

- 前面完成参数解析
- 中间释放 GIL 跑重活
- 后面重新拿回 GIL 组装结果

这样边界更清晰，也更容易审计正确性。

---

### 最佳实践 3：后台线程碰 Python 前先 acquire

这是硬规则，不是建议。

---

### 最佳实践 4：特别小心 `py::object` 的跨线程存活和析构

构造、移动、销毁，都要想 GIL。

---

### 最佳实践 5：不要把 GIL 当作你的业务数据锁

GIL 不能替代：

- `std::mutex`
- 读写锁
- 原子变量
- 明确的数据所有权模型

---

### 最佳实践 6：回调 Python 前尽量释放你自己的 C++ 锁

避免死锁和锁顺序反转。

---

## 21.18 一句话记忆钩子

```text
GIL 管的是 Python 世界；
离开 Python 世界去算纯 C++，可以放；
一旦回来碰对象、回调、引用计数、异常，就必须重新拿起。
```

---

# 22. pybind11 与零拷贝 / buffer / numpy：性能为什么可能很好

很多人用 pybind11 是为了性能，不只是“能调用 C++”。

但这里要先把一个概念说准：

**零拷贝不等于零开销。**

它真正表示的是：

```text
大块数据本体没有复制
Python 和 C++ 看到的是同一块底层内存
```

即使是零拷贝，通常仍然会有一些轻量开销：

- 创建 view / wrapper 对象
- 读取 shape / stride / dtype 元数据
- 做边界检查或类型检查

真正省下来的，是最贵的那部分：

- 大数组复制
- 大图像复制
- 大矩阵复制

---

## 22.1 最理想情况

```text
Python 数据
   │
   ├── 不复制底层 bytes
   ▼
C++ 直接看同一块 buffer
```

比如 NumPy array，本质上就是：

```text
NumPy array
├── data pointer
├── dtype
├── ndim
├── shape
└── strides
```

如果 pybind11 能拿到这些信息，就可以：

- 不做大规模数据复制
- 让 C++ 直接访问底层数据
- 必要时还支持切片/转置后的 strided 视图

---

## 22.2 一个关键概念：buffer protocol 到底提供了什么

Python 世界里，很多对象都能暴露“我背后有一块连续或分步长的原始内存”。

这套统一接口就是 **buffer protocol**。

它核心上提供这些元数据：

- `ptr`：数据起始地址
- `itemsize`：每个元素多少字节
- `format`：元素类型格式，例如 `float64`
- `ndim`：维度数
- `shape`：每一维大小
- `strides`：每一维步长（单位通常是字节）

可视化：

```text
buffer_info
├── ptr     -> 0x....
├── format  -> double
├── ndim    -> 2
├── shape   -> [rows, cols]
└── strides -> [row_stride, col_stride]
```

在 pybind11 里，和它最相关的几个工具是：

- `py::buffer`
- `py::buffer_info`
- `py::array` / `py::array_t<T>`
- `py::buffer_protocol()`
- `.def_buffer(...)`

一句话：

```text
buffer protocol 负责描述内存
pybind11 负责把这套描述映射到 C++
```

---

## 22.3 先看最常见场景：Python -> C++ 零拷贝读 NumPy

比如你想让 C++ 直接读取 Python 传来的 `float64` 一维数组。

### 最严格、最适合高性能内核的写法

```cpp
m.def("sum_1d", [](py::array_t<double, py::array::c_style> arr) {
    if (arr.ndim() != 1) {
        throw std::runtime_error("expected a 1D float64 array");
    }

    auto v = arr.unchecked<1>();
    double s = 0.0;
    for (py::ssize_t i = 0; i < v.shape(0); ++i) {
        s += v(i);
    }
    return s;
});
```

### 这段代码的关键点

- `py::array_t<double, py::array::c_style>`：要求元素类型是 `double`，并且是 C contiguous
- 没有写 `forcecast`：不想让 pybind11 为了“帮你适配”偷偷创建副本
- `unchecked<1>()`：拿一个轻量 view，少掉重复边界检查开销

### 为什么这更接近真正的零拷贝

如果 Python 传入的是：

```python
a = np.arange(5, dtype=np.float64)
```

那么 C++ 看到的就是 `a` 的底层内存本体，不需要再复制一份。

但如果用户传的是：

```python
a = np.arange(5, dtype=np.float32)
```

或者：

```python
a = np.arange(10, dtype=np.float64)[::2]
```

那么这类严格接口会直接拒绝，而不是悄悄复制后再继续。

这正是高性能 API 的一个好习惯：

```text
宁可明确拒绝不符合布局的数据
也不要偷偷帮用户复制一份大数组
```

---

## 22.4 一个特别常见的坑：`forcecast` 可能让你“看起来零拷贝，实际偷偷复制”

很多人第一次写会这样：

```cpp
m.def("scale_bad", [](py::array_t<double> arr, double alpha) {
    auto v = arr.mutable_unchecked<1>();
    for (py::ssize_t i = 0; i < v.shape(0); ++i) {
        v(i) *= alpha;
    }
});
```

表面看没问题，但要小心：

- `py::array_t<double>` 的默认模板参数带有 `forcecast` 语义
- 如果传入的不是精确匹配的数组类型，pybind11 可能先构造一个临时兼容数组

比如：

```python
a = np.arange(5, dtype=np.float32)
m.scale_bad(a, 2.0)
```

你可能以为是在原地改 `a`，但实际上可能发生的是：

```text
float32 array
   │
   ▼
临时转成 float64 array
   │
   ▼
C++ 改的是临时副本
   │
   ▼
返回后临时对象销毁
原数组 a 没被原地修改
```

### 最佳实践

- **性能敏感 API**：默认不用 `forcecast`
- **真正的 inplace API**：要求 dtype 和 layout 严格匹配
- **教学/演示 API**：如果为了易用性保留 `forcecast`，要在文档里明确说明“可能复制”

---

## 22.5 如果你想支持切片/转置也零拷贝，就必须理解 stride

并不是只有“连续数组”才能零拷贝。

例如：

```python
a = np.arange(12, dtype=np.float64).reshape(3, 4)
b = a[:, ::2]
c = a.T
```

- `b` 往往是 strided view
- `c` 往往也是 view，只是 stride 变了
- 它们都可能仍然共享原始 data pointer

视觉化：

```text
原数组 a
├── ptr -> same memory block
├── shape = [3, 4]
└── strides = [32, 8]

切片/转置后的 view
├── ptr -> still same memory block
├── shape = different
└── strides = different
```

所以：

- **连续** 只是零拷贝的一种理想布局
- **strided view** 也可以是零拷贝
- 真正决定你能不能安全处理它的，是你是否正确使用 `shape + strides`

---

## 22.6 通用 buffer 写法：接受 strided 输入而不复制

如果你想支持更一般的 buffer provider，而不仅仅是 NumPy，可以直接吃 `py::buffer`。

```cpp
double sum_buffer(py::buffer b) {
    py::buffer_info info = b.request();

    if (info.ndim != 1) {
        throw std::runtime_error("expected a 1D buffer");
    }
    if (info.format != py::format_descriptor<double>::format()) {
        throw std::runtime_error("expected float64 data");
    }

    auto *ptr = static_cast<double *>(info.ptr);
    const auto stride = info.strides[0] / static_cast<py::ssize_t>(sizeof(double));

    double s = 0.0;
    for (py::ssize_t i = 0; i < info.shape[0]; ++i) {
        s += ptr[i * stride];
    }
    return s;
}
```

### 这个例子说明了什么

- 你没有要求 contiguous
- 你没有复制数据
- 你是按 stride 去读，所以切片也能正确处理

### 适用场景

- 想兼容 NumPy、memoryview、array.array 等多种 buffer provider
- 算法本身能接受非连续布局
- 你更看重通用性，而不是最极限的 SIMD / cache 友好连续访问

---

## 22.7 原地修改的最佳实践例子

如果你要写“原地修改数组”的接口，建议契约更严格。

```cpp
m.def("scale_inplace", [](py::array_t<double, py::array::c_style> arr, double alpha) {
    if (arr.ndim() != 1) {
        throw std::runtime_error("expected a 1D float64 array");
    }

    auto v = arr.mutable_unchecked<1>();
    for (py::ssize_t i = 0; i < v.shape(0); ++i) {
        v(i) *= alpha;
    }
});
```

Python 侧：

```python
x = np.arange(5, dtype=np.float64)
m.scale_inplace(x, 10.0)
# x 被原地修改
```

### 为什么这是好实践

因为它表达得很清楚：

- 我就是要你给我一个真正的 `float64` 数组
- 我就是要连续布局
- 我就是要直接改这块内存
- 不满足条件就报错，不偷偷复制

这类 API 的心智模型最稳定。

---

## 22.8 反方向：C++ -> Python 怎么做到零拷贝

这时关键问题变成：

```text
Python 返回去的 array view 指向哪块 C++ 内存？
这块内存谁拥有？
它什么时候释放？
```

### 一个正确的零拷贝返回例子：用 capsule 绑定生命周期

```cpp
m.def("make_array", []() {
    constexpr py::ssize_t n = 8;
    auto *data = new double[n];
    for (py::ssize_t i = 0; i < n; ++i) {
        data[i] = static_cast<double>(i);
    }

    py::capsule owner(data, [](void *p) {
        delete[] static_cast<double *>(p);
    });

    return py::array_t<double>(
        {n},
        {static_cast<py::ssize_t>(sizeof(double))},
        data,
        owner
    );
});
```

### 为什么这是零拷贝

- Python 返回的 `ndarray` 直接指向 `data`
- 没有额外复制出第二份数组内容
- `capsule` 负责在最后一个 Python view 死掉时释放 `data`

### 视觉化

```text
C++ heap buffer data
   ▲
   │ shared same pointer
NumPy array view
   │
   ▼
base/owner = capsule
```

---

## 22.9 一个危险反例：不要把局部容器的 `data()` 借给 Python

下面这种写法非常危险：

```cpp
m.def("bad_array", []() {
    std::vector<double> v{1, 2, 3, 4};
    return py::array_t<double>(
        {static_cast<py::ssize_t>(v.size())},
        {static_cast<py::ssize_t>(sizeof(double))},
        v.data()
    );
});
```

为什么错：

```text
函数返回时
局部变量 v 被销毁
v.data() 立刻悬空
Python 拿到的是 dangling pointer
```

这类 bug 很隐蔽，因为：

- 可能第一次 print 还“看起来正常”
- 但后面随机崩溃或读到脏数据

### 最佳实践

- 不要把 stack / 局部 `std::vector` / 临时对象的内部指针直接借给 Python
- 返回 view 时，一定显式绑定 owner
- owner 可以是 `py::capsule`，也可以是某个真正拥有内存的 Python/C++ 对象

---

## 22.10 自定义 C++ 类如何把内部内存暴露给 Python：`def_buffer`

如果你有自己的矩阵类：

```cpp
class Matrix {
public:
    double *data();
    std::size_t rows() const;
    std::size_t cols() const;
};
```

可以这样暴露 buffer protocol：

```cpp
py::class_<Matrix>(m, "Matrix", py::buffer_protocol())
    .def_buffer([](Matrix &mat) -> py::buffer_info {
        const auto rows = static_cast<py::ssize_t>(mat.rows());
        const auto cols = static_cast<py::ssize_t>(mat.cols());
        return py::buffer_info(
            mat.data(),
            sizeof(double),
            py::format_descriptor<double>::format(),
            2,
            {rows, cols},
            {
                static_cast<py::ssize_t>(sizeof(double) * cols),
                static_cast<py::ssize_t>(sizeof(double))
            }
        );
    });
```

Python 侧就可以：

```python
mat = m.Matrix(...)
a = np.asarray(mat)
```

这时 `a` 往往就是对 `mat` 内部内存的一个 view。

### 这类方案的优点

- Python 代码自然，像在处理普通 ndarray
- C++ 不需要额外复制矩阵内容
- 你自己的类可以成为 NumPy 生态的一等公民

---

## 22.11 但 `def_buffer` 也有一个大坑：导出后底层内存不能乱搬家

假设你的 `Matrix` 内部其实用的是：

```cpp
std::vector<double> data_;
```

如果 Python 已经拿到了 view，而你后来又在 C++ 里做了会触发 reallocation 的操作，比如：

- `resize`
- `push_back`
- `reserve` 改变容量

那么原来的 `data()` 地址可能变掉，Python 这边的 view 就悬空了。

### 最佳实践

- 导出 buffer 后，尽量让底层存储地址稳定
- 如果对象会频繁扩容，避免长期导出裸 view
- 对外暴露只读视图时，通常更容易管理生命周期
- 真要可变共享，先把“谁能改大小、谁只能改内容”设计清楚

---

## 22.12 pybind11 零拷贝里最常见的“隐式复制”来源

很多人以为“我用了 `py::array_t`，所以就零拷贝了”，其实不一定。

常见隐式复制来源有：

### 1) dtype 不匹配

```text
Python 是 float32
C++ 想要 double
=> 可能发生转换复制
```

### 2) contiguity 不匹配

```text
Python 传入切片/转置 view
C++ 要求 c_style contiguous
=> 可能报错，或在 forcecast 下生成临时副本
```

### 3) 你把数组转成了 STL 容器

比如：

```cpp
std::vector<double> v = ...
```

这种思路通常就已经不是零拷贝了。

因为 STL caster 往往会做一份新的 C++ 容器副本。

### 4) 你返回了一个全新构造的 Python list / ndarray

只要你构造的是新内存，再把旧内容拷进去，那就不是零拷贝。

所以要区分：

```text
快
!=
零拷贝
```

---

## 22.13 什么时候该选“严格零拷贝”，什么时候该选“易用转换”

这是 API 设计问题，不只是技术问题。

### 严格零拷贝接口

适合：

- 热路径
- 大数组
- 原地修改
- 希望性能行为完全可预测

特点：

- dtype/layout 要求严格
- 不满足就报错
- 文档要明确写清楚约束

### 易用转换接口

适合：

- demo
- 低频调用
- 小数组
- 更看重“用户传什么都能跑”

特点：

- 可以接受 `forcecast`
- 可能自动复制/转型
- 但要清楚告知用户：这不是严格零拷贝 API

最怕的是中间态：

```text
文档说自己很高性能
实际却在不知不觉复制数据
```

---

## 22.14 最佳实践清单

### 最佳实践 1：先决定你的 API 契约

先明确这到底是哪一种：

- 必须严格零拷贝
- 尽量零拷贝，但允许退化复制
- 完全为了方便，复制也没关系

不要把这三种语义混在一起。

---

### 最佳实践 2：性能敏感接口默认不用 `forcecast`

对热路径，宁可明确报错，也不要悄悄复制大数组。

---

### 最佳实践 3：连续布局和任意 stride 是两类 API

- 如果你的算法要求连续内存，就明确要求 `c_style`
- 如果你的算法能处理 view/slice/transposed array，就按 stride 写循环

不要一边假设连续，一边又偷偷接收任意 view。

---

### 最佳实践 4：原地修改接口要格外严格

inplace API 最怕“看起来改了，实际改的是临时副本”。

所以：

- dtype 要严格
- layout 要明确
- 生命周期要稳定

---

### 最佳实践 5：返回 C++ 内存给 Python 时，一定显式绑定 owner

常见 owner：

- `py::capsule`
- 导出的宿主对象本身
- 其他真正拥有这块内存的对象

没有 owner 的 view，几乎总是在埋雷。

---

### 最佳实践 6：不要把“局部对象内部指针”暴露给 Python

包括：

- 局部 `std::vector`
- 局部数组
- 临时对象返回的内部缓冲区

这些都很容易变成悬空指针。

---

### 最佳实践 7：如果你只是想快，不一定非要零拷贝

有时候：

- 数据量不大
- 算法非常重
- 拷贝成本相对可忽略

那你完全可以接受一次复制，换更简单稳健的 API。

真正该坚持零拷贝的，是：

- 数据很大
- 调用很频繁
- 内存带宽是瓶颈
- 需要原地共享

---

## 22.15 一句话总结

```text
pybind11 的零拷贝本质不是“神奇地更快”
而是“让 Python 与 C++ 共享同一块底层内存，并把 dtype / shape / strides / ownership 说清楚”
```

如果你只记住一个判断标准，就记这个：

**有没有复制数据本体，以及这块共享内存的生命周期是否被明确、安全地托管。**

---

# 23. 如果你要“重新发明 pybind11”，最小可行产品怎么做？

不要一上来想完整 pybind11。你需要先发明一个最小 binding system。

## 23.1 第 1 步：只支持绑定一个纯 C 函数

目标：

```cpp
int add(int, int)
```

你手写包装器：

```cpp
PyObject* wrapped_add(PyObject*, PyObject* args) {
    int a, b;
    // 从 args 取 a,b
    // 调 add(a,b)
    // 返回 Python int
}
```

### 你已经得到什么

- 理解了 Python 调 C 的 ABI 边界
- 理解了参数解析和返回封装

## 23.2 第 2 步：做一个注册表

你不想手写每个方法表项，于是做：

```text
name -> wrapper function pointer
```

类似：

```cpp
register_function("add", wrapped_add);
```

## 23.3 第 3 步：做参数转换器

现在你不想每个函数都手写 `int a, b` 解析。

于是设计：

```cpp
template <typename T>
T from_python(PyObject* obj);

template <typename T>
PyObject* to_python(T&& value);
```

这就是最原始的 **type caster 雏形**。

## 23.4 第 4 步：自动从函数签名生成包装器

如果函数签名是：

```cpp
R(*)(Args...)
```

那么包装逻辑统一是：

```text
1. 取 Python 实参列表
2. 逐个转成 Args...
3. 调用真实函数
4. 把 R 转回 Python
```

这一步一旦模板化，你就从“手写胶水”迈入“binding library”。

## 23.5 第 5 步：加入类绑定

你需要：

- Python type object
- 存放 C++ instance pointer
- 构造/析构桥接
- 方法调用桥接

## 23.6 第 6 步：加入生命周期策略

没有这一步，你的库只能 demo，不能工程使用。

你要明确：

- raw pointer
- unique_ptr
- shared_ptr
- borrowed reference
- reference_internal

## 23.7 第 7 步：加入异常、重载、kwargs、默认参数

到这里，你就离 pybind11 的用户体验很近了。

---

# 24. Python 与 pybind11 的关系图

```text
Python 语言
   ▼
CPython 运行时
   ├── 字节码 VM
   ├── 对象模型
   ├── 引用计数 / GC
   ├── GIL
   └── C API
          ▼
      pybind11
   ├── 函数绑定
   ├── 类绑定
   ├── type caster
   ├── 生命周期管理
   ├── 异常翻译
   └── GIL 管理
          ▼
      C++ 业务代码 / 算法库
```

一句话总结关系：

- **CPython 提供原始接口（C API）**
- **pybind11 把这些原始接口包装成更友好的 C++ 绑定工具**

---

# 25. 多个例子串起来看

# 25.1 例子 A：绑定纯函数

## Python 侧想要的效果

```python
m.add(1, 2)
```

## pybind11 做的事

```text
Python int(1) ─┐
               ├─> 转成 C++ int -> 调 add -> 转回 Python int
Python int(2) ─┘
```

## 本质问题

- 参数转换
- 返回值转换

---

# 25.2 例子 B：绑定字符串函数

```cpp
std::string hello(std::string name) {
    return "hello, " + name;
}
```

## 底层关键点

```text
Python str -> std::string
std::string -> Python str
```

## 本质问题

- Unicode / 编码
- 内存所有权

---

# 25.3 例子 C：绑定类

```cpp
class Counter {
public:
    Counter(int v) : v(v) {}
    void inc() { ++v; }
    int value() const { return v; }
private:
    int v;
};
```

## Python 侧

```python
c = m.Counter(5)
c.inc()
print(c.value())
```

## 底层图

```text
Python Counter object
└── 持有 C++ Counter* / holder
        ├── v = 5
        └── 调 inc()/value()
```

## 本质问题

- Python 对象如何关联到 C++ 实例
- 析构时机谁负责

---

# 25.4 例子 D：返回引用的危险

```cpp
class Zoo {
public:
    Pet& pet() { return pet_; }
private:
    Pet pet_;
};
```

如果 Python 拿到的是内部成员引用，而 `Zoo` 提前销毁，就会悬空。

## 本质问题

- “这个返回值是不是独立拥有的对象？”
- “还是借来的内部视图？”

这就是 return value policy 存在的原因。

---

# 26. 一个非常重要的思维方式：不要把 pybind11 看成“魔法”，要看成“边界工程”

很多初学者会问：

> 为什么几行 `m.def(...)` 就能工作？

正确答案不是“它很神奇”，而是：

```text
它把重复的边界代码抽象掉了。
```

边界代码包括：

- 参数检查
- 类型转换
- 异常翻译
- 生命周期管理
- 调用约定对接
- Python 运行时注册

所以 pybind11 的本质不是“让 C++ 变 Python”，而是：

**系统化地管理两种运行时之间的边界。**

---

# 27. 如果要从零教学，我会怎么带你重新发明这两者

这里给你一条非常适合 visual learner 的路线。

## 27.1 先重发明 Python（玩具版）

### 阶段 1：表达式树

做这个：

```text
1 + 2 * 3
```

画出 AST：

```text
   (+)
  /   \
 1    (*)
     /   \
    2     3
```

### 阶段 2：栈式执行器

把 AST 编译成：

```text
PUSH 1
PUSH 2
PUSH 3
MUL
ADD
```

### 阶段 3：加入对象头

给每个值加：

```text
refcnt + type
```

### 阶段 4：加入函数与 frame

### 阶段 5：加入 list/dict 这样的容器

### 阶段 6：加入循环 GC

这样你会真正明白：

- 解释器不是“读一行执行一行”那么简单
- 它其实是一个完整运行时系统

## 27.2 再重发明 pybind11（玩具版）

### 阶段 1：手写一个 `wrapped_add`

### 阶段 2：把 `int` 的参数解析封成函数

### 阶段 3：把返回值封装封成模板

### 阶段 4：支持任意 `R(Args...)`

### 阶段 5：支持类和对象 holder

### 阶段 6：支持 shared_ptr 与异常翻译

这样你会真正明白：

- pybind11 不是“一个语法糖头文件”
- 它是一个很成熟的跨运行时绑定框架

---

# 28. 最后做一个总对照表

| 主题 | Python / CPython 在解决什么 | pybind11 在解决什么 |
|---|---|---|
| 核心目标 | 执行 Python 语言 | 连接 Python 与 C++ |
| 运行时主体 | 解释器 + VM + 对象系统 | 胶水层 + 类型转换层 |
| 通用数据单位 | `PyObject*` | `PyObject*` 与 C++ 类型之间的映射 |
| 调用问题 | 怎样执行函数调用 | 怎样把 Python 调用翻译成 C++ 调用 |
| 内存问题 | 引用计数 + GC | 所有权/生命周期策略 |
| 多态问题 | 类型对象 / 方法槽 | 模板推断 + 运行时注册 + trampoline |
| 并发问题 | GIL | 什么时候持有/释放 GIL |
| 扩展能力 | C API / 扩展模块 | 更现代的 C++ 封装接口 |

---

# 29. 一页纸总结

## Python（准确说 CPython）底层最重要的 6 件事

1. **源码不会直接执行**，而是要经过解析、编译、字节码、VM。  
2. **几乎一切都是对象**，对象有统一头部，带类型信息。  
3. **动态性来自运行时分派**，操作会根据对象类型走不同实现。  
4. **内存管理以引用计数为主**，再用 GC 处理循环引用。  
5. **函数调用依赖 frame**，frame 保存局部变量、栈和返回位置。  
6. **GIL 是解释器级线程保护机制**，简化了内部一致性。  

## pybind11 底层最重要的 6 件事

1. 它建立在 **CPython C API** 上，不是替代 Python 运行时。  
2. 它的本质是 **自动化胶水代码生成**。  
3. 它的核心机制是 **type caster**。  
4. 它大量利用 **C++ 模板** 从函数签名生成包装逻辑。  
5. 真正难点不只是“能调通”，而是 **生命周期 / 所有权 / GIL / 异常**。  
6. 如果你能手写一个最小 wrapper，再逐步抽象，就能“重新发明 pybind11”。  

---

# 30. 给 visual learner 的最后建议

如果你要真正吃透，不要只看定义，按下面顺序画图：

## 先画 Python

1. `x = 1 + 2` 的 AST 图  
2. 同一句代码的字节码栈变化图  
3. `PyObject(refcnt, type)` 对象头图  
4. frame 调用栈图  
5. 循环引用图  

## 再画 pybind11

1. `m.def("add", &add)` 的调用链图  
2. `PyObject* -> C++ int -> PyObject*` 转换图  
3. Python wrapper 持有 C++ instance 的对象图  
4. shared_ptr 所有权图  
5. GIL 持有/释放边界图  

如果你能把这些图默画出来，说明你已经从“会用”走到“理解原理”了。

---

# 31. 一个极简记忆钩子

```text
Python/CPython = 如何执行 Python
pybind11       = 如何把 C++ 接到 Python 上

Python 的核心：对象 + 字节码 VM + 引用计数/GC + GIL
pybind11 的核心：wrapper + type caster + ownership + GIL bridge
```

---

如果下一步你愿意，我建议继续做两篇配套文档：

1. **用 100 行伪代码重写一个玩具 Python VM**  
2. **用最小 C API 手写一个 add 模块，再对照 pybind11 看它省掉了什么**
