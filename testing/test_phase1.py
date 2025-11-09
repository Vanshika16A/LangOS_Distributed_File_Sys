import socket
import time
import sys

def register_as_storage_server(ns_ip, ns_port, ss_ip, client_port, files):
    """Simulates a Storage Server registering with the Name Server."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((ns_ip, ns_port))
            file_list = ",".join(files)
            message = f"REGISTER_SS;{ss_ip};{client_port};{file_list}\n"
            s.sendall(message.encode('utf-8'))
            print(f"-> Sent SS registration: {message.strip()}")
    except Exception as e:
        print(f"Error registering Storage Server: {e}")

def register_as_client(ns_ip, ns_port, username):
    """Simulates a User Client registering with the Name Server."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((ns_ip, ns_port))
            message = f"REGISTER_CLIENT;{username}\n"
            s.sendall(message.encode('utf-8'))
            print(f"-> Sent Client registration: {message.strip()}")
    except Exception as e:
        print(f"Error registering Client: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 test_phase1.py <NameServer_IP> <NameServer_Port>")
        sys.exit(1)

    NS_IP = sys.argv[1]
    NS_PORT = int(sys.argv[2])

    print("--- Starting Phase 1 Automated Test ---")
    
    # Test Case 1: Register one Storage Server
    print("\n[TEST] Registering Storage Server 1...")
    register_as_storage_server(NS_IP, NS_PORT, "192.168.1.10", 9500, ["report.pdf", "main.c"])
    time.sleep(1)

    # Test Case 2: Register one Client
    print("\n[TEST] Registering Client 'bob'...")
    register_as_client(NS_IP, NS_PORT, "bob")
    time.sleep(1)

    # Test Case 3: Register another SS (with no files)
    print("\n[TEST] Registering Storage Server 2 (no files)...")
    register_as_storage_server(NS_IP, NS_PORT, "192.168.1.11", 9501, [])
    time.sleep(1)
    
    # Test Case 4: Register another client
    print("\n[TEST] Registering Client 'charlie'...")
    register_as_client(NS_IP, NS_PORT, "charlie")

    print("\n--- Test Complete ---")
    print("Check the Name Server's console output for verification.")