#!/usr/bin/python3

#
# This script should be run at the top of MUDFISH directory.
#

import getopt
import os
import sys

def readLastUID():
    fp = open(".bandec_lastuid", "r")
    uid = fp.read()
    fp.close()
    return int(uid)

def writeLastUID(uid):
    fp = open(".bandec_lastuid", "w")
    fp.write(str(uid))
    fp.close()

def main():
    try:
          opts, args = getopt.getopt(sys.argv[1:], "p:v")
    except(getopt.GetoptError, err):
          print(str(err))
          sys.exit(1)
    p_arg = "."
    v_flag = False
    for o, a in opts:
          if o == "-p":
              p_flag = a
          elif o == "-v":
              v_flag = True
          else:
              assert False, "unhandled option"

    magic = [ "BANDEC_XXXXX" ]

    uid = readLastUID()
    for dirname, dirnames, filenames in os.walk(p_arg):
          #for subdirname in dirnames:
          #    print os.path.join(dirname, subdirname)
          for filename in filenames:
              path = os.path.join(dirname, filename)
              fname, fext = os.path.splitext(path)
              if fext != ".c" and fext != ".cc" and fext != ".h" and \
                 fext != ".js" and \
                 fext != ".kt" and \
                 fext != ".m" and \
                 fext != ".php" and \
                 fext != ".rs" and \
                 fext != ".swift" and \
                 fext != ".tsx":
                    continue

              #
              # Read input file.
              #
              fp = open(path, "r")
              body = fp.read()
              fp.close()

              #
              # Replace our magic.
              #
              occur = 0
              for m in magic:
                    while body.find(m) != -1:
                        body = body.replace(m, "BANDEC_%05d" % uid, 1)
                        uid += 1
                        occur += 1
              if occur == 0:
                    if v_flag == True:
                        print("Skiped %s" % path)
                    continue
              else:
                    print("Converted %s (%d times)" % (path, occur))
              #
              # Write output file
              #
              fp = open(path, "w")
              fp.write(body)
              fp.close()
    writeLastUID(uid)

if __name__ == "__main__":
    main()
