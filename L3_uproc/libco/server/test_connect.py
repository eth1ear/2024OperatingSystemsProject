import socket
import threading

SERVER_IP = "127.0.0.1"  # 服务器的IP地址
SERVER_PORT = 12345  # 服务器的端口号
MESSAGE = "Hello, Echo Server!"
MAX_THREADS = 8192 * 10  # 尝试的最大连接数


def client_thread():
    try:
        # 创建一个socket
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((SERVER_IP, SERVER_PORT))

        # 发送消息
        s.sendall(MESSAGE.encode())
        data = s.recv(1024)

        # 检查收到的消息是否正确
        if data.decode() != MESSAGE:
            print("Error: Received wrong data!")

        s.close()
    except Exception as e:
        print(f"Error: {e}")


threads = []

for i in range(MAX_THREADS):
    t = threading.Thread(target=client_thread)
    t.start()
    threads.append(t)

# 等待所有线程完成
for t in threads:
    t.join()

print("Test finished!")
