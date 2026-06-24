Import("env")

def after_upload(source, target, env):
    env.Execute("screen /dev/ttyUSB* 115200")
    env.Execute("reset")

env.AddPostAction("upload", after_upload)
