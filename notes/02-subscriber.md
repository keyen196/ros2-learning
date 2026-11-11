# ROS2 第 2 课：订阅者节点（发布 → 订阅闭环）

## 本课目标

在 arm_hello 包里新增第二个节点 `hello_subscriber`：订阅 `/hello_topic`，把收到的消息
打印出来。与第 1 课的发布者同时运行，亲眼看到"你发我收"——消息闭环打通。

## 类比：电台与听众

发布者=电台主播（对频道喊话），订阅者=听众（调到同一频道，主播说一句这边播一句）。
和 `ros2 topic echo` 的区别：echo 是临时路人听客，订阅节点是**长期驻扎在程序里的听众**，
收到消息后可执行任意逻辑（本课打印，以后可以是驱动机械臂）。

## 新增源码文件

路径：`~/arm_ws/src/arm_hello/src/hello_subscriber.cpp`

### 第 1 块：头文件 + main 骨架

```cpp
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

int main(int argc, char **argv)
{
    return 0;
}
```

- 和第 1 课完全一样——头文件只跟"用什么数据类型、用哪个库"有关，与发还是收无关

### 第 2 块：初始化 + 创建节点

```cpp
rclcpp::init(argc, argv);
auto node = std::make_shared<rclcpp::Node>("hello_subscriber");
RCLCPP_INFO(node->get_logger(), "订阅节点已启动");
```

- 节点名 `hello_subscriber` 与发布者的 `hello_node` 不同名 → `ros2 node list` 能看到两个独立节点
- 其余与第 1 课一模一样

### 第 3 块：订阅者 + 回调（本课核心）

```cpp
auto subscriber = node->create_subscription<std_msgs::msg::String>(
    "hello_topic", 10,
    [node](const std_msgs::msg::String::SharedPtr msg)
    {
        RCLCPP_INFO(node->get_logger(), "收到: %s", msg->data.c_str());
    });
```

- `create_subscription<String>` 四件套：
  - 模板类型 `std_msgs::msg::String`：要收的消息类型，**必须与发布者一致**，否则互相看不见
  - 话题名 `"hello_topic"`：**必须与发布者一模一样**（错一个字母都收不到，且双方不报错——静默失败）
  - 队列深度 `10`：回调处理不过来时最多缓存 10 条，满了丢最旧
  - 回调：每收到一条消息触发一次（详见"学员问答实录"Q1）
- 回调参数 `msg`：ROS 在 spin 里检测到消息时自动塞进来的，类型
  `std_msgs::msg::String::SharedPtr`（指向 String 消息的共享指针），`const` = 只读承诺

### 第 4 块：spin + 收尾

```cpp
rclcpp::spin(node);
rclcpp::shutdown();
return 0;
```

- **没有 spin，回调登记了也永不触发**（spin 才是检测事件、调用回调的那个），程序会直接退出

### 完整源码

```cpp
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<rclcpp::Node>("hello_subscriber");
    RCLCPP_INFO(node->get_logger(), "订阅节点已启动");

    auto subscriber = node->create_subscription<std_msgs::msg::String>(
        "hello_topic", 10,
        [node](const std_msgs::msg::String::SharedPtr msg)
        {
            RCLCPP_INFO(node->get_logger(), "收到: %s", msg->data.c_str());
        });

    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
```

## CMakeLists.txt 追加 3 行

在 `ament_package()` 之前、已有 3 行的下面再加：

```cmake
add_executable(hello_subscriber src/hello_subscriber.cpp)
ament_target_dependencies(hello_subscriber rclcpp std_msgs)
install(TARGETS hello_subscriber
  DESTINATION lib/${PROJECT_NAME})
```

- 模板与第 1 课相同，只换程序名和源文件名 → 现在一个包里两个可执行程序

## 编译与运行

```bash
cd ~/arm_ws
colcon build                      # 预期 Summary: 1 package finished
source install/setup.bash         # 每次编译后必须重新 source
```

闭环验证（开 2-3 个终端，每个都先 source 两条）：

| 终端 | 命令 | 预期 |
|---|---|---|
| A | `ros2 run arm_hello hello_publisher` | 每秒"发布: Hello from arm_hello!" |
| B | `ros2 run arm_hello hello_subscriber` | 每秒"收到: Hello from arm_hello!"，与 A 同步滚动 |
| C（可选） | `ros2 node list` / `ros2 topic echo /hello_topic` | 两个节点都在；第三方也能收到——一个话题可同时有多个订阅者 |

B 与 A 内容同步滚动 = 发布→订阅链路真实打通。

## 发布者 vs 订阅者 对照表

| | create_publisher | create_subscription |
|---|---|---|
| 模板类型 | 要发的消息类型 | 要收的消息类型（必须一致） |
| 参数1 | 话题名 | 话题名（必须一模一样） |
| 参数2 | 队列深度（发出后缓存） | 队列深度（回调处理不过来时缓存，丢最旧） |
| 参数3 | 无 | **回调**：每收到一条消息触发一次 |
| 类比 | 主播 | 听众 |

## 学员问答实录（本课实问，重点复习）

### Q1：怎么理解 `[node]`？"回调"体现在哪里？

- `[node]` 是 **lambda 的捕获列表**：lambda 是被派出去晚点执行的小函数，函数体默认看不见
  外面的变量，方括号=背包，`[node]`=把 node 塞进背包，函数体里才能用。[] 里写谁才能用谁，
  没写的用了会编译报错。第 1 课用 `[node, publisher]` 是因为函数体用了两样。
- **回调不是一个语法关键字——整个 lambda 就是回调**。判断标准：不是被你直接调用、而是
  登记给系统、事件发生时由系统反过来调用。时间线：登记（不执行）→ spin 值班 → 消息到达
  → spin 调用 lambda 并塞入 msg → 打印 → 返回继续等。"回调"=调用方向反过来了（系统→你）。
- 与定时器回调同模式，只是触发事件不同：时间到点 vs 消息到达。

### Q2：共享指针和普通指针的区别？为什么用在这里？

- 指针=地址条，指向内存里的对象。普通指针（裸指针）=光秃秃的地址条：忘 delete→内存泄漏；
  delete 早了别人还在用→野指针崩溃。
- 共享指针=地址条自带**计数**：每多一人持有 +1，每少一人 -1，**归零自动释放**——
  会议室关灯类比：进门 +1 出门 -1，最后走的人自动关灯（释放内存）。无人需要写 delete。
- 这里必须用的三个理由：① node 被多处共用（spin/登记/lambda 捕获）；② 回调随时可能被
  触发，对象生命周期不确定——捕获 shared_ptr 把对象寿命与回调绑定，回调还在对象必在，
  捕获裸指针则可能野指针崩溃；③ rclcpp 全按 shared_ptr 设计（回调参数类型就叫
  String::SharedPtr），跟随官方风格。
- `msg` 参数同理：SharedPtr=所有权共享给你但不用你管释放，const=只读承诺。

## 复述批改记录（第 2 课）

- 题1：答出类型/话题名/队列深度，**漏第 4 要素"回调"**（第三个参数，本课核心）→ 已由 Q1 补上
- 题2：本质对（话题塞给我的）；精确类型 `std_msgs::msg::String::SharedPtr`，
  比 std_msgs 多包一层 SharedPtr
- 题3：对——两个独立进程经同名话题+同类型消息完成真实跨进程通信

## 易错点

- 话题名不一致（错一个字母）→ **静默失败**：双方都不报错，就是收不到。用 `ros2 topic list` 对照
- 消息类型不一致 → 双方互相看不见对方
- 忘了 spin → 回调登记了但永不触发，程序秒退

## 本课新增 C++ 语法

- 捕获列表 `[node]`：[] 写谁，函数体里就能用谁
- `const std_msgs::msg::String::SharedPtr msg`：指向消息的共享指针 + 只读
- 回调三步：登记 → spin 值班 → 事件触发系统调用

---

> 下一课预告：把节点写成"类"——现代 ROS2 标准写法（成员变量、this、std::bind 登场），
> 学完就能读懂大多数 ROS2 教程代码。
