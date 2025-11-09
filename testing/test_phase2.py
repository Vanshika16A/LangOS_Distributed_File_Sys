import socket
import sys
import time

def send_and_receive(ns_ip, ns_port, command):
    """A helper to connect, send a command, and receive the full response."""
    response = ""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((ns_ip, ns_port))
            s.sendall(command.encode('utf-8'))
            
            # Keep receiving data until the __END__ token is found
            while True:
                data = s.recv(1024)
                if not data:
                    break
                response += data.decode('utf-8')
                if "__END__" in response:
                    break
        # Clean up the response
        return response.split("__END__")[0].strip()
    except Exception as e:
        return f"ERROR: {e}"

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 test_phase2.py <NameServer_IP> <NameServer_Port>")
        sys.exit(1)

    NS_IP = sys.argv[1]
    NS_PORT = int(sys.argv[2])
    
    print("--- Starting Phase 2 Automated Test ---")

    # Step 1: Register a Storage Server to populate files
    print("\n[TEST] Registering Storage Server...")
    ss_reg_cmd = "REGISTER_SS;127.0.0.1;9001;main.c,report.txt,data.csv\n"
    send_and_receive(NS_IP, NS_PORT, ss_reg_cmd) # We don't need the response
    print("  -> Done.")
    time.sleep(0.5)

    # Step 2: Register two users
    print("\n[TEST] Registering Users 'alice' and 'bob'...")
    send_and_receive(NS_IP, NS_PORT, "REGISTER_CLIENT;alice\n")
    send_and_receive(NS_IP, NS_PORT, "REGISTER_CLIENT;bob\n")
    print("  -> Done.")
    time.sleep(0.5)

    # Step 3: Test LIST_USERS command
    print("\n[TEST] Sending LIST_USERS command...")
    response = send_and_receive(NS_IP, NS_PORT, "LIST_USERS;\n")
    print("--- Server Response ---")
    print(response)
    print("-----------------------\n")
    time.sleep(0.5)

    # Step 4: Test VIEW command (simple)
    print("[TEST] Sending VIEW command (simple)...")
    response = send_and_receive(NS_IP, NS_PORT, "VIEW;-\n")
    print("--- Server Response ---")
    print(response)
    print("-----------------------\n")
    time.sleep(0.5)
    
    # Step 5: Test VIEW command (with details)
    print("[TEST] Sending VIEW command (with -l flag)...")
    response = send_and_receive(NS_IP, NS_PORT, "VIEW;-l\n")
    print("--- Server Response ---")
    print(response)
    print("-----------------------\n")
    
    print("--- Test Complete ---")
    print("Please verify the output above is correct.")