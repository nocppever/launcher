Import("env")

def after_upload(source, target, env):
    print("Setting boot partition to factory...")
    from platformio import fs
    import subprocess
    
    upload_port = env.GetProjectOption("upload_port")
    
    cmd = [
        "esptool.py",
        "--chip", "esp32",
        "--port", upload_port,
        "--baud", "921600",
        "write_flash",
        "0xe000",    # Address of otadata partition
        "0xFF" * 8   # Write 8 bytes of 0xFF to clear OTA data
    ]
    
    try:
        subprocess.run(cmd, check=True)
        print("Boot partition set to factory successfully")
    except Exception as e:
        print(f"Warning: Failed to set boot partition: {e}")

env.AddPostAction("upload", after_upload)