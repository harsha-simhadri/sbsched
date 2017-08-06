#!/bin/bash
#Usage: cbox_percore.sh <core> <event_selection_string> <program>
event_str_prefix=' -e uncore_cbox_'
event_str_postfix='/'$2'/ '

for i in {0..7}
do
event_str+=$event_str_prefix
event_str+=$i
event_str+=$event_str_postfix
done

complete_string='sudo perf_3.6 stat -x, -a -C '$1$event_str$3
echo Running: 
echo $complete_string
$complete_string
