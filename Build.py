import platform
import os
import argparse
import string

def main():
    parser = argparse.ArgumentParser(description='Build Mozi')
    parser.add_argument('--target', default="VS2019", help='the target to build(Doc, VS2022, VS2019, XCode)')
    args = parser.parse_args()
     
    systemName = platform.system()
    print("Building On " + systemName + " System")

    dir_path = os.path.dirname(os.path.realpath(__file__))
    print("Current Path is " + dir_path)

    if not os.path.isdir(dir_path + '/Build/'):
        os.mkdir(dir_path + '/Build/')

    if not os.path.isdir(dir_path + '/Bin/'):
        os.mkdir(dir_path + '/Bin/')

    os.chdir(dir_path + "/Build")
    if systemName == "Windows":
        if args.target == "Doc":
            os.chdir(dir_path)
            os.system('.\Tools\Doxygen\Win64\doxygen.exe')
        elif args.target == "VS2022":
            os.system('cmake -G "Visual Studio 17 2022" -A x64 ../')
        else:
            os.system('cmake -G "Visual Studio 16 2019" -A x64 ../')
    elif systemName == "Darwin":
        os.system('cmake -G "Xcode" ../')

if __name__ == '__main__':
    main()