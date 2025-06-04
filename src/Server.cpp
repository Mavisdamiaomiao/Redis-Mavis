#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "future_task_dispatcher.h" // 里面#include <thread>
#include <vector>
#include <algorithm> // std::transform 配合 std::toupper转大写
#include <cctype> // 使用 std::toupper()

// 解析 RESP 命令为字符串数组
std::vector<std::string> parse_redis_command(const std::string& input) {
  std::vector<std::string> result;
  size_t pos = 0;

  // 检查是否以 * 开头
  if (input[pos] != '*') {
    std::cerr << "Invalid RESP format: missing '*'" << std::endl;
    return result;
  }
  pos++; // 跳过 '*'

  // 解析元素数量
  size_t end = input.find("\r\n", pos);
  if (end == std::string::npos) {
    std::cerr << "Invalid RESP format: missing CRLF after array size" << std::endl;
    return result;
  }
  int count = std::stoi(input.substr(pos, end - pos));
  pos = end + 2;

  for (int i = 0; i < count; ++i) {
    // 检查是否以 $
    if (input[pos] != '$') {
        std::cerr << "Invalid RESP format: expected '$'" << std::endl;
        return result;
    }
    pos++; // 跳过 '$'

    // 解析字符串长度
    end = input.find("\r\n", pos);
    if (end == std::string::npos) {
      std::cerr << "Invalid RESP format: missing CRLF after bulk string length" << std::endl;
      return result;
    }
    int len = std::stoi(input.substr(pos, end - pos));
    pos = end + 2;

    // 提取字符串内容
    if (pos + len > input.size()) {
      std::cerr << "Invalid RESP format: bulk string out of bounds" << std::endl;
      return result;
    }
    std::string value = input.substr(pos, len);
    result.push_back(value);
    pos += len + 2; // 跳过内容和结尾的 \r\n
  }

  return result;
}

// 构造 RESP Bulk String 响应
std::string build_bulk_string(const std::string& value) {
  return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
}

// 处理客户端连接
void handle_client(int client_fd) {
  while (true) {
    char buffer[1024] = {0};
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
      break; // 客户端关闭连接或发生错误
    }

    std::string input(buffer);
    auto parts = parse_redis_command(input);

    if (!parts.empty()) {
      std::string cmd = parts[0];
      // 转成大写（命令大小写不敏感）
      std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

      if (cmd == "ECHO" && parts.size() == 2) {
        std::string reply = build_bulk_string(parts[1]);
        send(client_fd, reply.c_str(), reply.size(), 0);
      } else if (cmd == "PING") {
        std::string pong = "+PONG\r\n";
        send(client_fd, pong.c_str(), pong.size(), 0);
      }
      else {
        std::string error = "-ERR unknown command\r\n";
        send(client_fd, error.c_str(), error.size(), 0);
      }
    }

  }

  close(client_fd);
}


int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf; // 每次输出后都刷新缓冲区，unitbuf为I/O操纵符,方便调试
  std::cerr << std::unitbuf;
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  std::cout << "Server listening on port 6379...\n";

  while (true) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
    if (client_fd < 0) {
      std::cerr << "Failed to accept connection\n";
      continue;
    }

    std::cout << "Client connected\n";

    // 多线程处理每个客户端连接
    // std::thread t(handle_client, client_fd);
    // t.detach(); // 分离线程，自动运行和回收资源

    std::thread t(handle_client, client_fd);
    t.detach();
  }

  close(server_fd);
  return 0;
}
