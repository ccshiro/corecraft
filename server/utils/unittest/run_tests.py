# TODO: It might be better to solve this by using gtest's event listener API,
#       so that the binaries produce less output by default. And then just run
#       them using the CMake's custom target.

import os
import subprocess
import sys

if len(sys.argv) < 3:
  print("No tests to run.")
  sys.exit(1)

rpath = sys.argv[1]
tests = sys.argv[2:]
success = 0
errors = 0

for test in tests:
  modded_env = os.environ.copy()
  if len(rpath) > 0:
    modded_env['LD_LIBRARY_PATH'] = rpath

  p = subprocess.Popen([test, "--gtest_shuffle", "--gtest_color=no"],
    shell=True, bufsize=-1, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    env=modded_env)
  (pout, perr) = p.communicate()
  
  inside_run = False
  err_lines = ""
  
  for line in pout.splitlines():
    line = line.decode('utf-8')
    if line.startswith("{FATAL_ERROR}:"):
      print(line)
      sys.exit(-1)
    elif line.startswith("[ RUN      ]"):
      inside_run = True
      err_lines = ""
    elif inside_run:
      if line.startswith("[       OK ]"):
        inside_run = False
        success = success + 1
      else:
        if line.startswith("[  FAILED  ]"):
          errors = errors + 1
          inside_run = False
          print("")
          print(line)
          print(err_lines)
        else:
          err_lines += line + os.linesep

# results of all tests combined
print(str(success) + " tests passed.")
print(str(errors) + " tests failed.")
