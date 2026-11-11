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
