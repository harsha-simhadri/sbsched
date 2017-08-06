#!/usr/bin/python
import os
from subprocess import Popen, PIPE
import sys
import datetime
import socket

# sudo python collect-data.py <#runs> <prog> <"input params">

args = sys.argv
path = "test/" + args[2]
sockets = "all"

active_time = []
l3_misses = []

hostname = socket.gethostname().partition('.')[0]
filename = "timings/"+hostname+"/"+args[2]+"_runs_"+args[1]
for more_args in range(3, len(sys.argv)):
  filename = filename + "_" + args[more_args]
filename = filename+"_sock_"+sockets+"_"
dt = datetime.datetime.now()
compact_dt = "_%s.%s.%s.%s.%s.%s" % (dt.year,dt.month,dt.day,dt.hour,dt.minute,dt.second)
filename = filename+compact_dt

fout = open(filename,"w")

for runs in range(0, int(args[1])):
  command = " hugectl --heap=2M numactl -i "+ sockets + " " + path 
  for more_args in range(3, len(sys.argv)):
    command = command + " " + args[more_args]
  print command
  process = Popen(command, stdout=PIPE, stderr=PIPE, shell=True)
  exit_code = os.waitpid(process.pid, 0)
  output = str(process.communicate()[0]).splitlines()
  for line in output:
    if line.startswith("T: "):
      words =  line.split(" ")
      active_time.append(int(words[1]))
      print active_time
    if line.startswith("L3_MISSES: "):
      words =  line.split(" ")
      l3_misses.append(int(words[1]))
      print l3_misses
      

fout.write("\n")
for runs in range(0, int(args[1])):
  fout.write(str(active_time[runs]))
  fout.write("   ")

fout.write("\n")
for runs in range(0, int(args[1])):
  fout.write(str(l3_misses[runs]))
  fout.write("   ")
fout.write("\n")

active_time_filt = active_time
l3_misses_filt = l3_misses


fout.write("\n ")
fout.write(" ============\n")
active_time_filt.remove(max(active_time_filt))
active_time_filt.remove(min(active_time_filt))
sum=0.0
average=0.0
for n in active_time_filt:
  sum = sum + n
  average = sum/len(active_time_filt)
print "Avg Active Time: %.0f ms" % average
fout.write("Avg Active Time: ")
fout.write(str(average))
fout.write("\n")

l3_misses_filt.remove(max(l3_misses_filt))
l3_misses_filt.remove(min(l3_misses_filt))
sum=0.0
average=0.0

for n in l3_misses_filt:
  sum = sum + n
  average = sum/(1000000*len(l3_misses_filt))
print "Avg L3 misses: %.1f mln" % average
fout.write("Avg L3_misses: ")
fout.write(str(average))
fout.write("\n")

fout.close()
