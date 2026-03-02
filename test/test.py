import socket
import requests
import time

def check_diag():
    target_host = "127.0.0.1"
    target_port = 11435
    api_url = "https://api.codegeex.cn"

    print("--- CodeGeeX Agent 诊断开始 ---")

    # 1. 检查本地端口是否开放
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(2)
        result = s.connect_ex((target_host, target_port))
        if result == 0:
            print(f"[√] 本地端口 {target_port} 已开启 (Agent 进程可能正在运行)")
        else:
            print(f"[×] 本地端口 {target_port} 未响应 (Agent 进程未启动或被拦截)")

    # 2. 尝试与 Agent 进行握手 (Health Check)
    try:
        response = requests.get(f"http://{target_host}:{target_port}/health", timeout=3)
        print(f"[√] Agent 响应成功: {response.status_code} {response.text}")
    except Exception as e:
        print(f"[×] 无法与本地 Agent 通信: {e}")

    # 3. 检查云端 API 连通性 (测试是否被 Clash 拦截)
    try:
        start_time = time.time()
        res = requests.get(api_url, timeout=5)
        elapsed = (time.time() - start_time) * 1000
        print(f"[√] 云端 API 连通正常 (延迟: {elapsed:.2f}ms)")
    except Exception as e:
        print(f"[×] 无法访问云端 API，请检查 Clash 规则: {e}")

    print("--- 诊断结束 ---")

if __name__ == "__main__":
    check_diag()