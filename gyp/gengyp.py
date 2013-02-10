import sys, os.path

# This script reads .gyp files and generates apropriate .sln etc.
# projects and solutions. Gyp can generate solution for every version
# of Visual Studio
try:
    import gyp
except:
    print("You need to install gyp from")
    print("http://code.google.com/p/gyp/\n\n")
    raise

if __name__ == '__main__':
    fp = os.path.join("gyp", "sizer.gyp")
    # TODO: I don't get why the generated .sln etc. files are put in
    # ${--generator-output}/gyp directory
    args = [fp, "--generator-output=.", "-G", "msvs-version=2010", "--suffix=-vs2010"]
    ret = gyp.main(args)
    sys.exit(ret)
