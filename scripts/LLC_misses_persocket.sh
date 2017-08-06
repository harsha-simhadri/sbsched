#!/bin/bash
event_str_prefix=' -e uncore_cbox_'
event_str_postfix='/event=0x14,umask=111/ '

for i in {0..7}
do
event_str+=$event_str_prefix
event_str+=$i
event_str+=$event_str_postfix
done

rm -f __dump 
ls > /dev/null  #To avoid hysteresis in programs"
complete_string='sudo perf_3.6 stat -o __dump -x, -a -C '$1$event_str$2
echo Running: 
#echo $complete_string
$complete_string
cat __dump | tail -n 8 | awk -F"," '{print $1}' | awk '{total+=$0}END{print "L3_miss(1socket): "(total)/1000000"M"}' 
rm -f __dump 
ls > /dev/null  #To avoid hysteresis in programs"
