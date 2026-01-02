import subprocess
import time
import os
import sys

SERVER_BIN = "./build/server"
CLIENT_BIN = "./build/client"
DISK_IMG = "test_net.img"

def run_client(commands):
    p = subprocess.Popen([CLIENT_BIN], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    out, err = p.communicate(input=commands)
    return out, err

def test():
    # Clean up
    if os.path.exists(DISK_IMG):
        os.remove(DISK_IMG)
    
    # Start Server
    print(f"Starting server on port 9090 with disk {DISK_IMG}")
    server = subprocess.Popen([SERVER_BIN, "-d", DISK_IMG, "-p", "9090"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    time.sleep(2) # Wait for server to start

    try:
        # 1. Register Reviewer 1 (AI Expert)
        print("Step 1: Register Reviewer 1 (AI)")
        cmds = """connect 127.0.0.1 9090
register rev1 pass1 reviewer
login rev1 pass1
profile "MIT" "AI, ML"
logout
quit
"""
        out, err = run_client(cmds)
        if "Profile updated successfully" not in out:
            print("FAILED: Reviewer 1 profile update")
            print("OUTPUT:", out)
            print("ERROR:", err)
            return

        # 2. Register Reviewer 2 (Systems Expert)
        print("Step 2: Register Reviewer 2 (Systems)")
        cmds = """connect 127.0.0.1 9090
register rev2 pass2 reviewer
login rev2 pass2
profile "Stanford" "Systems, DB"
logout
quit
"""
        out, err = run_client(cmds)
        if "Profile updated successfully" not in out:
            print("FAILED: Reviewer 2 profile update")
            print("OUTPUT:", out)
            return

        # 3. Register Author and Submit Paper (AI Paper)
        print("Step 3: Register Author and Submit Paper")
        # Create a dummy pdf file
        with open("paper.pdf", "wb") as f:
            f.write(b"dummy content")
            
        cmds = """connect 127.0.0.1 9090
register auth1 pass1 author
login auth1 pass1
submit "AI Paper" "Abstract about AI" "AI" paper.pdf
logout
quit
"""
        out, err = run_client(cmds)
        if "Paper submitted successfully" not in out:
            print("FAILED: Paper submission")
            print("OUTPUT:", out)
            print("ERROR:", err)
            return

        # 4. Register Editor and Auto Assign
        print("Step 4: Register Editor and Auto Assign")
        cmds = """connect 127.0.0.1 9090
register edit1 pass1 editor
login edit1 pass1
autoassign 1
logout
quit
"""
        out, err = run_client(cmds)
        if "Auto-assignment triggered successfully" not in out:
            print("FAILED: Auto assignment")
            print("OUTPUT:", out)
            return
            
        print("SUCCESS: All network features verified!")

    finally:
        server.terminate()
        try:
            server.wait(timeout=2)
        except subprocess.TimeoutExpired:
            server.kill()
            
        if os.path.exists("paper.pdf"):
            os.remove("paper.pdf")
        if os.path.exists(DISK_IMG):
            os.remove(DISK_IMG)

if __name__ == "__main__":
    test()
