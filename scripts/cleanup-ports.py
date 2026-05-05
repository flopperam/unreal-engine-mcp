import subprocess
import sys
import re
import os

def cleanup_ports(ports):
    """Find and kill processes using the specified ports (Windows only)."""
    if os.name != 'nt':
        print("This script currently only supports Windows.")
        return

    print(f"Cleaning up ports: {', '.join(map(str, ports))}")
    
    for port in ports:
        try:
            # Find PID using netstat
            output = subprocess.check_output(f'netstat -aon | findstr :{port}', shell=True).decode()
            pids = set()
            for line in output.strip().split('\n'):
                # Extract PID from the last column
                match = re.search(r'\s+(\d+)\s*$', line)
                if match:
                    pids.add(match.group(1))
            
            for pid in pids:
                if pid == '0': continue
                print(f"Killing process {pid} using port {port}...")
                subprocess.run(f'taskkill /F /PID {pid}', shell=True, check=False)
        except subprocess.CalledProcessError:
            # findstr returns exit code 1 if no matches found
            pass
        except Exception as e:
            print(f"Error cleaning up port {port}: {e}")

if __name__ == "__main__":
    target_ports = [55557, 8787, 8000]
    if len(sys.argv) > 1:
        target_ports = [int(p) for p in sys.argv[1:]]
    cleanup_ports(target_ports)
    print("Cleanup complete.")
