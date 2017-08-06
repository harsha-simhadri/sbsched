#!/usr/bin/python
import os
from subprocess import Popen, PIPE
import sys
import datetime

# sudo python collect-data.py <#runs> <prog> <"input params">

args = sys.argv
path = "test/" + args[2]
scheds = ["W", "P", "2", "5"]

active_time = [[] for sched_id in xrange(len(scheds))]
overhead = [[] for sched_id in xrange(len(scheds))]
l3_misses = [[] for sched_id in xrange(len(scheds))]

filename = "newdata2/"+args[1]+"_"+args[2]
for more_args in range(3, len(sys.argv)):
  filename = filename + "_" + args[more_args]
dt = datetime.datetime.now()
compact_dt = "_%s.%s.%s.%s.%s.%s" % (dt.year,dt.month,dt.day,dt.hour,dt.minute,dt.second)
filename = filename+compact_dt
print filename
fout = open(filename,"w")

for sched_id in range(0, len(scheds)):
  for runs in range(0, int(args[1])):
    command = "sudo hugectl --heap=2M numactl -i 0 "+ path + " " + scheds[sched_id] 
    for more_args in range(3, len(sys.argv)):
      command = command + " " + args[more_args]
    print command
    process = Popen(command, stdout=PIPE, stderr=PIPE, shell=True)
    exit_code = os.waitpid(process.pid, 0)
    output = str(process.communicate()[0]).splitlines()
    for line in output:
      if line.startswith("ms_Active: "):
        words =  line.split(" ")
        active_time[sched_id].append(int(words[1]))
        print active_time
      if line.startswith("ms_Overhead: "):
        words =  line.split(" ")
        overhead[sched_id].append(int(words[1]))
        print overhead

for sched_id in range(0, len(scheds)):
  fout.write("\n")
  for runs in range(0, int(args[1])):
    fout.write(str(active_time[sched_id][runs]))
    fout.write("   ")

for sched_id in range(0, len(scheds)):
  fout.write("\n")
  for runs in range(0, int(args[1])):
    fout.write(str(overhead[sched_id][runs]))
    fout.write("   ")

fout.write("\n")

active_time_filt = active_time
overhead_filt = overhead
l3_misses_filt = l3_misses

for sched_id in range(0, len(scheds)):
  print "====== %s ======" % scheds[sched_id]
  fout.write("\n ")
  fout.write(scheds[sched_id])
  fout.write(" ============\n")
  active_time_filt[sched_id].remove(max(active_time_filt[sched_id]))
  active_time_filt[sched_id].remove(min(active_time_filt[sched_id]))
  sum=0.0
  average=0.0
  for n in active_time_filt[sched_id]:
    sum = sum + n
  average = sum/len(active_time_filt[sched_id])
  print "Avg Active Time: %.0f ms" % average
  fout.write("Avg Active Time: ")
  fout.write(str(average))
  fout.write("\n")

  overhead_filt[sched_id].remove(max(overhead_filt[sched_id]))
  overhead_filt[sched_id].remove(min(overhead_filt[sched_id]))
  sum=0.0
  average=0.0
  for n in overhead_filt[sched_id]:
    sum = sum + n
  average = sum/len(overhead_filt[sched_id])
  print "Avg Overhead Time: %.0f ms" % average
  fout.write("Avg overhead: ")
  fout.write(str(average))
  fout.write("\n")

fout.close()
