# ROS2 第 1 课：手写第一个 C++ Publisher 节点

## 本课目标

从零创建一个 ROS2 功能包，写一个 C++ 节点：**每秒向话题 `/hello_topic` 发布一条消息
"Hello from arm_hello!"**，并用命令行工具从"旁观者"视角验证消息真实在网络中传输。

完整流水线（以后每个 C++ 节点都是这条链）：

```
建包 → 写代码 → 改 CMakeLists → colcon build → source → ros2 run → topic echo 验证
```

## 环境

- Windows 11 + WSL2 + Ubuntu 22.04
- ROS2 Humble
- 工作空间 `~/arm_ws`（src/ 放包，build/install/log 由 colcon 生成）

## 第 1 步：创建功能包

```bash
cd ~/arm_ws/src
ros2 pkg create arm_hello --build-type ament_cmake --dependencies rclcpp std_msgs
```

参数逐个看：

- `arm_hello`：包名，命名规则全小写 + 下划线
- `--build-type ament_cmake`：C++ 包用 CMake 构建（Python 包是 ament_python）
- `--dependencies rclcpp std_msgs`：声明依赖，自动写进 package.xml 和 CMakeLists.txt

`rclcpp` 是 ROS2 的 C++ 库，`std_msgs` 是标准消息包（含 String 类型）。

### 功能包里两个核心配置文件

| 文件 | 类比 | 谁读它 | 作用 |
|---|---|---|---|
| package.xml | 身份证 | colcon | 登记"我是谁、我依赖谁" |
| CMakeLists.txt | 菜谱 | CMake | 声明"编什么文件、链接哪些库、装到哪" |

`rclcpp` 在两个文件里都出现，**不重复**：package.xml 是"声明需要"，CMakeLists.txt 是"真的接入"。

## 第 2 步：编写节点代码（分 3 块）

文件路径：`~/arm_ws/src/arm_hello/src/hello_publisher.cpp`

### 第 1 块：头文件 + main 骨架

```cpp
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

int main(int argc, char **argv)
{
    return 0;
}
```

**解释：**

- `rclcpp/rclcpp.hpp`：ROS2 C++ 主头文件，几乎每个节点都要包含
- `std_msgs/msg/string.hpp`：String 消息类型的定义，路径规则 = `包名/msg/消息名.hpp`

### 第 2 块：初始化 + 创建节点

```cpp
int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<rclcpp::Node>("hello_node");
    RCLCPP_INFO(node->get_logger(), "节点已启动");

    return 0;
}
```

**逐行解释：**

- `rclcpp::init(argc, argv)` — 通知 ROS2 底层通信系统（DDS）"本程序要开始用 ROS 了"。
  把 argc/argv 传进去是因为 ROS2 支持在启动命令里附带配置。每个节点的 main 开头都是这句。
- `::` 双冒号 — 读作"属于"：`rclcpp::init` = "rclcpp 库里的 init 函数"。
- `auto node = std::make_shared<rclcpp::Node>("hello_node");` — 创建节点对象，拆开看：
  - `auto`：让编译器根据右边内容自动推断变量类型。完整类型是
    `std::shared_ptr<rclcpp::Node>`，很长，用 auto 省事且不易写错。
  - `"hello_node"`：**节点名**。注意区分：`arm_hello` 是包名（存放代码的单位），
    `hello_node` 是节点名（运行中的程序实例）。一个包可含多个节点。
  - `make_shared`：用"共享指针"管理对象——多处共同持有、最后持有者用完自动释放。
    节点对象接下来要交给多个环节共用，所以 ROS2 官方示例都用这个写法。现阶段当固定套路。
- `RCLCPP_INFO(node->get_logger(), "...")` — ROS 风格日志打印，自带时间戳和节点名，
  会被 ROS 日志系统收集。
- `->` 箭头 — 变量是指针类型时，访问它内部的东西用箭头；普通变量用点 `.`。

### 第 3 块：发布者 + 定时器 + spin

```cpp
auto publisher = node->create_publisher<std_msgs::msg::String>("hello_topic", 10);

auto timer = node->create_wall_timer(
    std::chrono::seconds(1),
    [node, publisher]()
    {
        std_msgs::msg::String msg;
        msg.data = "Hello from arm_hello!";
        RCLCPP_INFO(node->get_logger(), "发布: %s", msg.data.c_str());
        publisher->publish(msg);
    });

rclcpp::spin(node);
rclcpp::shutdown();
```

**解释：**

- `create_publisher<std_msgs::msg::String>("hello_topic", 10)`
  - `<std_msgs::msg::String>`：发什么类型的消息
  - `"hello_topic"`：发到哪个话题，命名规则同包名
  - `10`：队列深度，消息没被及时取走最多缓存 10 条，满了丢最旧
- `create_wall_timer` + lambda — **定时器 + 回调**，本课核心机制
  - `std::chrono::seconds(1)`：间隔 1 秒
  - `[node, publisher](){...}` 是 lambda：就地写的小函数，方括号是"捕获列表"，
    意思是"这个小函数要用到外面的 node 和 publisher，先抓进来"
  - **回调（callback）**：这个小函数不是现在执行，而是登记给定时器；每到 1 秒
    由系统反过来调用它一次。"你不直接调它、事件发生时系统调它"，所以叫回调。
    ROS2 的订阅、服务、机械臂控制全建立在这套"事件触发"模式上。
- `msg.data = "..."` — String 消息内部就一个字段 `data`，类型 `std::string`。
- `rclcpp::spin(node)` — **值班循环**：前面所有代码只是在"布置"（建节点、发布者、闹钟），
  spin 让程序真正运转：停在这一行不退出，事件一来就处理。**没有它，定时器永远不会响，
  程序直接跑到 return 0 退出**。Ctrl+C 后 spin 返回。
- `rclcpp::shutdown()` — 收尾清理，然后 return 0 正常退出。

### 完整源码

```cpp
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<rclcpp::Node>("hello_node");
    RCLCPP_INFO(node->get_logger(), "节点已启动");

    auto publisher = node->create_publisher<std_msgs::msg::String>("hello_topic", 10);

    auto timer = node->create_wall_timer(
        std::chrono::seconds(1),
        [node, publisher]()
        {
            std_msgs::msg::String msg;
            msg.data = "Hello from arm_hello!";
            RCLCPP_INFO(node->get_logger(), "发布: %s", msg.data.c_str());
            publisher->publish(msg);
        });

    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
```

## 第 3 步：CMakeLists.txt 补 3 行

打开 `~/arm_ws/src/arm_hello/CMakeLists.txt`，在 `ament_package()` 这一行**之前**插入：

```cmake
add_executable(hello_publisher src/hello_publisher.cpp)
ament_target_dependencies(hello_publisher rclcpp std_msgs)
install(TARGETS hello_publisher
  DESTINATION lib/${PROJECT_NAME})
```

| 行 | 作用 | 关键点 |
|---|---|---|
| `add_executable` | 把 cpp 编译成可执行程序 | 第一个词 `hello_publisher` 是自己起的，运行时要用 |
| `ament_target_dependencies` | 真正接入 rclcpp / std_msgs 的头文件和库 | 对应 package.xml 的"声明需要" |
| `install(...)` | 把编译产物复制到 `install/lib/arm_hello/` | **漏了这行编译照样成功，但 ros2 run 找不到程序** |

`${PROJECT_NAME}` 是变量，值 = `project(arm_hello)` 里的 arm_hello，所以装到 `lib/arm_hello/`。
用变量不写死，包改名也不用改这行。

## 第 4 步：编译

```bash
cd ~/arm_ws
colcon build
```

**预期输出：**

```
Starting >>> arm_hello
Finished <<< arm_hello [XX.Xs]

Summary: 1 package finished [XX.Xs]
```

- 必须在 `~/arm_ws` 下执行——colcon 到当前目录的 src/ 找包
- 首次编译十几秒正常，之后改动重编只需几秒
- 失败时找**第一次出现 error 的地方**，后面的报错往往是连带的

## 第 5 步：运行

```bash
cd ~/arm_ws
source install/setup.bash     # 每次编译后必须重新 source
ros2 run arm_hello hello_publisher
```

- `source install/setup.bash` 把编译产物的路径写进当前终端的环境变量——
  不 source，ros2 run 找不到你的新包
- `ros2 run 包名 可执行程序名`：包名来自 package.xml，程序名来自 add_executable 第一个词

**预期输出：** 每秒一条 `[INFO] ... [hello_node]: 发布: Hello from arm_hello!`，Ctrl+C 停止。

## 第 6 步：旁观者视角验证（开第二个终端）

**第一个终端的节点保持运行**，新开一个终端：

```bash
source /opt/ros/humble/setup.bash     # ROS 本体环境
source ~/arm_ws/install/setup.bash    # 自己工作空间的环境

ros2 node list           # 应看到 /hello_node
ros2 topic list          # 应看到 /hello_topic /parameter_events /rosout
ros2 topic echo /hello_topic    # 每秒打印一条消息
ros2 topic hz /hello_topic      # average rate ≈ 1.0
```

- `ros2 topic echo` 本身就是一个临时订阅者——直观展示"发布者/订阅者"关系
- `/parameter_events` 和 `/rosout` 是 ROS2 给每个节点**自动配**的标准话题
  （参数事件、日志通道），认识即可

## 核心概念速查

| 概念 | 一句话 |
|---|---|
| 功能包 | ROS2 代码最小组织单位 |
| package.xml | 身份证，登记"我是谁、需要谁" |
| CMakeLists.txt | 菜谱，声明"编什么、用哪些库、装到哪" |
| 节点 | 运行中的程序实例，一个包可含多个节点 |
| 话题 | 节点间传消息的命名通道 |
| 发布者 | 往话题发消息的对象 |
| 回调 + spin | 事件驱动：登记小函数，spin 是值班循环 |
| colcon + source | colcon 编译工作空间；source 把产物登记进当前终端环境 |

## 易错点排错表

| 症状 | 原因 |
|---|---|
| `No package found` | 没 source 工作空间，或 CMakeLists 漏 install 行 |
| `No executable found` | 运行名 ≠ add_executable 第一个词 |
| 找不到头文件 | 代码拼写错，或 ament_target_dependencies 缺 |
| 大片语法错误 | 中文引号 / 中文分号混入代码 |
| 新终端找不到自己的包 | 新终端必须 source 两条（ROS 本体 + 工作空间） |

## 本课出现的 C++ 语法备忘

- `::` "属于"；`auto` 自动推断类型；指针用 `->` 取内部、普通变量用 `.`
- `shared_ptr` / `make_shared`：共享指针，自动管理内存，多处共用
- lambda `[捕获](){...}`：就地小函数，`[]` 里写要抓进来的外部变量
- 注释：C++ 用 `//`，CMake 用 `#`；代码里严禁中文标点

---

> 下一课预告：订阅者节点 `hello_subscriber`，订阅 `/hello_topic` 把消息打印出来——
> 发布 → 订阅完整闭环。
