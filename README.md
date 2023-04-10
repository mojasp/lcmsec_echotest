# performance analysis of the LCMSEC group discovery protocol

## Goals

* Measuring time to perform the protocol is a bit useless -> it is bound by timeouts
* Instead, measure number of JOIN_Responses it takes to form consensus. Afterwards, ensure that the db algorithm is successful
* Why JOIN_Responses? => 1) they are most expensive (biggest, due to number of certificates). 2) JOINs are always 2xParticipants (every participants transmits 2, one for group and once for channel). JOIN_Responses are the interesting part

## technical considerations

* We are running many nodes on one machine => its easy to overflow buffers since we are on a broadcast topology. also easy to time out due to unlucky context switches.
* Therefore we increase timeouts aaaaallll the way => use modified LCMSec version (gkexchg.h modified)
* We use tracy instrumentation to gather results => one instance connects to the tracy listener by linking to a special lcm.so. after the testrun, results are written to csv.
* demo_instances are used. doesn't matter what we do, as long as the certificates are valid

## Instructions for use / overview of the files in this directory
* all the files from lcm/examples/cpp_security are present: the demo_instance from there is the actual program being run. some are superflouus; i haven't cleaned it up.
* gen_certificates.py generates the PKI that is needed to run LCMSec
* gen_instances.sh generates the needed config files (need a different one for each instance). it uses sed, adjust the template_instance.toml file as needed for changes
* run_test.py runs a single run of the protocol. it takes #joining, #run(only needed for the name of the output file), directory(where to place output files), and #preexisting (to initialize a preexisting group before spinning up *joining*) as cmd-parameters
* run_all.py runs many run_test.py (self_explanatory python code)
* analysis.r to analyse the resulting .csv files and produce a graph
