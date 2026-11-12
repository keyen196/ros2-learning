# ROS2 第 3 课：节点写成类（现代 ROS2 标准写法）

## 本课目标

把第 2 课的订阅节点用类（class）风格重写一遍：功能不变，结构变为 ROS2 官方教程和
实际项目的主流写法。

**决策记录**：不删除 lambda 旧版，新建 `hello_subscriber_class.cpp` 两版并存——
①两版并排便于对照 ②可同时运行，实测"一个话题多个订阅者"。

**为什么换类写法**：lambda 版所有变量平铺在 main 里，回调需要用到哪个变量，就得在
捕获列表里逐个声明；类版把数据（成员变量）和函数（成员函数）组织在同一个对象里，
回调天然能访问成员，不需要逐个传递。变量一多，类写法明显更整洁。

## 源码分段解析

路径：`~/arm_ws/src/arm_hello/src/hello_subscriber_class.cpp`

### 段 1：头文件

```cpp
#include <functional>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
```

- `<functional>` 是新面孔：`std::bind` 是 C++ 标准库函数，声明在这个头文件里
- 另两个与第 1、2 课相同，作用不变

### 段 2：类声明 + 构造函数开头

```cpp
class HelloSubscriber : public rclcpp::Node
{
public:
    HelloSubscriber() : Node("hello_subscriber_class")
    {
        RCLCPP_INFO(this->get_logger(), "订阅节点已启动(类版)");
```

- `class HelloSubscriber : public rclcpp::Node` —— **继承**。写法含义：
  HelloSubscriber 自动拥有 rclcpp::Node 的全部成员函数和能力
  （create_subscription、get_logger、可被 spin 接受），在此之上再添加自己的成员。
  这是一个"is-a"关系：HelloSubscriber 是一种 Node。不写 `: public rclcpp::Node`
  它就是普通空类，没有任何 ROS 能力。
- `public:` —— 对外可见区域。构造函数要被 main 里的代码调用，必须放 public 区。
- `HelloSubscriber() : Node("hello_subscriber_class")` —— **构造函数**：
  对象被创建时自动执行的函数，原来 main 里的布置工作全部搬到这里。
  冒号后的部分叫**初始化列表**：先调用父类 Node 的构造函数、传入节点名，
  完成 Node 部分的初始化，然后才执行花括号里的函数体。
- 节点名用 `hello_subscriber_class`，与 lambda 版区分开；
  **话题名 hello_topic 不能改**，改了就与发布者匹配不上（静默收不到）。
- `this->get_logger()` —— `this` 指当前这个对象本身，`this->成员` 访问本对象的成员。
  语法上 `this->` 可省略，官方教程习惯保留，跟随即可。

### 段 3：创建订阅（本课核心行）

```cpp
        subscriber_ = this->create_subscription<std_msgs::msg::String>(
            "hello_topic", 10,
            std::bind(&HelloSubscriber::topic_callback, this,
                      std::placeholders::_1));
```

- `create_subscription` 的四要素与第 2 课完全相同：消息类型 String、话题名
  hello_topic、队列深度 10、回调。唯一区别：回调从 lambda 换成 `std::bind(...)`。
- **std::bind 三件套**：

| 部件 | 作用 |
|---|---|
| `&HelloSubscriber::topic_callback` | 要绑定的成员函数地址。成员函数必须写 `&类名::函数名`，因为成员函数属于类，只写函数名编译器不知道是哪个类的 |
| `this` | 绑定到当前对象——回调被触发时，在这个对象上调用这个函数 |
| `std::placeholders::_1` | 参数占位符：第一个参数的位置先空着，消息到达时由执行器把实际 msg 填进去 |

- bind 就是"绑定"：这一行把"函数 + 对象"打包，交给订阅对象保存；
  消息到达时执行器按这份绑定调用回调。绑定动作发生在登记这一刻。

### 段 4：回调成员函数（private 区）

```cpp
private:
    void topic_callback(const std_msgs::msg::String::SharedPtr msg)
    {
        RCLCPP_INFO(this->get_logger(), "收到: %s", msg->data.c_str());
    }
```

- `private:` —— 类内自用区域：回调函数、成员变量放这里，外部代码不能直接访问。
- 回调函数签名与第 2 课 lambda 版的参数**完全相同**
  （`const std_msgs::msg::String::SharedPtr msg`）：执行器触发时照样把消息传进来，
  变的只是登记方式（lambda → bind）。
- 不需要捕获列表的原因：成员函数属于对象，函数体内通过 this 可以直接访问本对象的
  全部成员，不存在"函数体看不到外部变量"的问题。

### 段 5：成员变量

```cpp
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscriber_;
```

- 详细拆解见下方问答 Q1。要点：这是指向"订阅器对象"的共享指针，
  String 只是 <> 里的模板参数（说明收什么消息），不是指针指向的对象。
- 命名尾部加 `_` 是 ROS2 社区惯例，用于区分成员变量和局部变量。
- 声明（本行，此刻为空）与赋值（段 3 构造函数里）是两步。

### 段 6：main

```cpp
int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<HelloSubscriber>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
```

- `std::make_shared<HelloSubscriber>()`：创建 HelloSubscriber 对象并纳入共享指针
  管理，详见问答 Q3。构造函数执行完毕时，订阅登记已全部完成。
- `spin(node)`：spin 的参数要求是 Node；HelloSubscriber 继承自 Node，
  所以可以直接传入（这就是继承带来的直接好处）。
- main 只剩四步：init → 造对象 → spin → shutdown。

## 完整源码（直接粘贴用）

```cpp
#include <functional>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

class HelloSubscriber : public rclcpp::Node
{
public:
    HelloSubscriber() : Node("hello_subscriber_class")
    {
        RCLCPP_INFO(this->get_logger(), "订阅节点已启动(类版)");
        subscriber_ = this->create_subscription<std_msgs::msg::String>(
            "hello_topic", 10,
            std::bind(&HelloSubscriber::topic_callback, this,
                      std::placeholders::_1));
    }

private:
    void topic_callback(const std_msgs::msg::String::SharedPtr msg)
    {
        RCLCPP_INFO(this->get_logger(), "收到: %s", msg->data.c_str());
    }

    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscriber_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<HelloSubscriber>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
```

## CMakeLists 追加 3 行（原有 6 行不动）

```cmake
add_executable(hello_subscriber_class src/hello_subscriber_class.cpp)
ament_target_dependencies(hello_subscriber_class rclcpp std_msgs)
install(TARGETS hello_subscriber_class
  DESTINATION lib/${PROJECT_NAME})
```

包内现有 3 个可执行程序：hello_publisher / hello_subscriber / hello_subscriber_class。

## 编译与验证

```bash
cd ~/arm_ws && colcon build && source install/setup.bash
```

| 终端 | 命令 | 预期 |
|---|---|---|
| A | `ros2 run arm_hello hello_publisher` | 每秒"发布: Hello from arm_hello!" |
| B | `ros2 run arm_hello hello_subscriber` | 每秒"收到: ..."（lambda 版） |
| C | `ros2 run arm_hello hello_subscriber_class` | 每秒"收到: ..."（类版） |
| D | `ros2 node list` | /hello_node /hello_subscriber /hello_subscriber_class |

B 和 C 同时收到同一条消息 = 一个话题可以挂多个订阅者，实测验证。

查看第 2 课旧版源码（git 历史里每个 commit 都保留着当时的文件内容）：

```bash
git -C ~/ros2-learning show 26c0995:arm_hello/src/hello_subscriber.cpp
```

## 两版对照表

| | lambda 版（第 2 课） | 类版（本课） |
|---|---|---|
| 回调形态 | 匿名函数 | 类的成员函数 |
| 回调怎么访问数据 | 捕获列表逐个声明 | 经 this 直接访问成员 |
| 登记方式 | 直接传 lambda | `std::bind(&类::函数, this, _1)` |
| main 里剩什么 | 全部布置平铺 | 造对象 + spin |
| ROS2 官方教程/实际项目 | 偶见 | 主流标准 |

## 学员问答实录

> 编号按本文件从 Q1 起；引用前面课程笔记的结论时，直接复述内容。

### Q1：`rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscriber_`
### 是"String 类型的指针"吗？

不是。这行要从里往外读，共四层：

| 层 | 片段 | 含义 |
|---|---|---|
| ① | `std_msgs::msg::String` | 模板参数：说明这个订阅器处理 String 类型的消息。它不是指针指向的对象 |
| ② | `rclcpp::Subscription<①>` | 完整类型：一个"订阅 String 消息的订阅器"对象，即 create_subscription 创建并返回的那个对象 |
| ③ | `::SharedPtr` | Subscription 类内部定义的共享指针别名，指向②这种对象 |
| ④ | `subscriber_` | 成员变量名 |

所以这行声明的是：**指向"订阅器对象"的共享指针**。容易误读的地方在于
`<>` 里的 String——它是消息类型的规格说明，不是被指向的东西。

`::SharedPtr` 是缩写，完整展开为：

```cpp
rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscriber_;
// 等价于：
std::shared_ptr<rclcpp::Subscription<std_msgs::msg::String>> subscriber_;
```

和第 2 课的关系：lambda 版里 `auto subscriber = node->create_subscription<...>(...)`
被 auto 隐藏的真实类型就是上面这串。本课只是把它显式写出、存成成员变量。

声明与赋值是两步：

```cpp
// ① 类内声明：此刻 subscriber_ 为空，未指向任何对象
rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscriber_;

// ② 构造函数内：创建订阅器对象，把它的（共享指针）赋给 subscriber_
subscriber_ = this->create_subscription<std_msgs::msg::String>(
    "hello_topic", 10, std::bind(...));
```

存它的意义：不存节点也能正常收消息（ROS 内部已经登记了这个订阅）；
存下来才持有句柄，之后才能做改队列深度、查订阅者数量、销毁订阅这类动态控制。

### Q2：shared_ptr 都在哪里被用了？

到目前为止共 5 处：

| # | 位置 | 代码 |
|---|---|---|
| 1 | 节点本体（第 1 课） | `std::make_shared<rclcpp::Node>("hello_node")` |
| 2 | 节点本体（本课） | `std::make_shared<HelloSubscriber>()` |
| 3 | 发布器/订阅器句柄 | create_publisher / create_subscription 的返回值（前两课用 auto 隐藏，本课显式写出 `::SharedPtr`） |
| 4 | 回调参数 | `const std_msgs::msg::String::SharedPtr msg` |
| 5 | 传给 spin | `rclcpp::spin(node)` |

为什么都用它（第 2 课笔记有完整展开，此处复述三个理由）：
① 这些对象都被多处长期引用——节点被 spin、回调登记、DDS 内部同时持有；
② 它们的生命周期由框架决定，裸指针容易在对象销毁后继续被使用（悬空指针，程序崩溃）；
③ rclcpp 的 API 整体按 shared_ptr 设计（回调参数类型本身就叫 String::SharedPtr），
跟随官方写法最不容易出错。

### Q3：`auto node = std::make_shared<HelloSubscriber>();` 怎么理解？

分三步：

1. **构造**：`HelloSubscriber()` 调用构造函数——先执行初始化列表
   `: Node("hello_subscriber_class")`（完成 Node 部分初始化、设节点名），
   再执行函数体（打印启动信息、登记订阅、给 subscriber_ 赋值）。
   这一整行执行完，节点已完全配置好。
2. **托管**：`std::make_shared<HelloSubscriber>()` 创建对象并立即返回
   `std::shared_ptr<HelloSubscriber>`，"创建 + 用共享指针管理"一步完成，
   比先创建再包装更高效也更安全。`auto node` 的真实类型就是
   `std::shared_ptr<HelloSubscriber>`。
3. **使用**：交给 `rclcpp::spin(node)`。spin 的参数要求是 Node，
   HelloSubscriber 继承自 Node，所以直接可传。

与第 1 课对比：第 1 课 `make_shared<rclcpp::Node>(...)` 创建的是通用 Node 对象；
本课创建的是自己的 HelloSubscriber 对象。main 的流程不变，只是造的东西换了。

## 复述批改记录（第 3 课）

1. `: public rclcpp::Node` —— 继承，获得 Node 全部能力。补全：不写则缺
   create_subscription / get_logger / 被 spin 接受，成普通空类
2. bind 三件套 —— 哪个函数（&类名::函数名）+ 绑哪个对象（this）+ 参数占位（_1），
   对
3. 类版回调不需要捕获 —— 成员函数属于对象，经 this 可访问全部成员，对
4. 话题名必须同为 hello_topic —— DDS 按"话题名+消息类型"匹配，不一致静默失败，对

## 本课新增 C++ 语法

- `class X : public Base` 继承；构造函数 + 初始化列表 `: Base(...)`
- `public:` / `private:` 两级可见性
- `this` 指针 = 当前对象本身
- `std::bind(&类::函数, this, std::placeholders::_1)`，头文件 `<functional>`
- 成员变量尾缀 `_` 惯例；类定义结尾 `};` 别忘分号

## 易错点

- 成员函数当回调必须写 `&类名::函数名`，只写函数名编译报错
- bind 里忘传 `this` → 编译错误：成员函数必须绑定对象
- 类定义结尾的 `};` 分号漏掉 → 报错定位在下一行，迷惑性强
- 类版节点名忘改会和 lambda 版同名（ros2 node list 分不清）
- 话题名改动 → 与发布者匹配不上，静默收不到（用 ros2 topic list 对照）

---

> 下一课预告：**自定义消息类型**——用 .msg 文件定义自己的消息（如关节角度），
> 并改造发布/订阅代码使用它；package.xml 与 CMakeLists 将新增消息相关的依赖项。
