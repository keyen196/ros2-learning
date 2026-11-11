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

