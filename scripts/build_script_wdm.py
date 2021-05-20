import datetime
import os       
import gzip
import shutil 
import subprocess

Import("env")

extension = ".gz"

subprocess.check_call('npm run build', shell=True);


def after_build_prog(source, target, env):  
    shutil.copyfile(env.subst("$BUILD_DIR") + "/firmware.bin", env.subst("$PROJECT_DIR") + "/output/Firmware_WDM.bin")
    print("GZIP Firmware File...")
    for subdir, dirs, files in os.walk(env.subst("$PROJECT_DIR") + "/output"):
        print("-> " + "Firmware_WDM.bin" + " to " + "Firmware_WDM.bin" + extension)
        with open(os.path.join(subdir, "Firmware_WDM.bin"), 'rb') as f_in, gzip.open(os.path.join(subdir, "Firmware_WDM.bin") + extension, 'wb') as f_out:
           f_out.writelines(f_in)
        os.remove(os.path.join(subdir, "Firmware_WDM.bin"))    
    print("")

env.AddPostAction("buildprog", after_build_prog)