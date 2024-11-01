Import("env")

# Override the upload address to force factory partition
env.Replace(
    UPLOADERFLAGS=[
        "--chip", "esp32",
        "--port", "$UPLOAD_PORT",
        "--baud", "$UPLOAD_SPEED",
        "--before", "default_reset",
        "--after", "hard_reset",
        "write_flash", 
        "-z",
        "--flash_mode", "dio",
        "--flash_freq", "80m",
        "--flash_size", "detect",
        "0x10000"
    ]
)

# Force bootloader and partition table upload
def before_upload(source, target, env):
    env.Execute("esptool.py --chip esp32 --port " + env.get('UPLOAD_PORT') + " --baud " + env.get('UPLOAD_SPEED') + " --before default_reset --after hard_reset erase_flash")
    env.Execute("esptool.py --chip esp32 --port " + env.get('UPLOAD_PORT') + " --baud " + env.get('UPLOAD_SPEED') + " --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x1000 $PROJECT_DIR/.pio/build/m5stack-core2/bootloader.bin 0x8000 $PROJECT_DIR/.pio/build/m5stack-core2/partitions.bin")

env.AddPreAction("upload", before_upload)