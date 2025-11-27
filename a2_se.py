import argparse
import os
import sys

import m5
from m5.defines import buildEnv
from m5.objects import *
from m5.params import NULL
from m5.util import (
    addToPath,
    fatal,
    warn,
)

from gem5.isas import ISA

addToPath("../gem5/configs/")

from common import (
    CacheConfig,
    CpuConfig,
    MemConfig,
    ObjectList,
    Options,
    Simulation,
)
from common.Caches import *
from common.cpu2000 import *
from common.FileSystemConfig import config_filesystem


def get_processes(args):
    """Interprets provided args and returns a list of processes"""

    multiprocesses = []
    inputs = []
    outputs = []
    errouts = []
    pargs = []

    workloads = args.cmd.split(";")
    if args.input != "":
        inputs = args.input.split(";")
    if args.output != "":
        outputs = args.output.split(";")
    if args.errout != "":
        errouts = args.errout.split(";")
    if args.options != "":
        pargs = args.options.split(";")

    idx = 0
    for wrkld in workloads:
        process = Process(pid=100 + idx)
        process.executable = wrkld
        process.cwd = os.getcwd()
        process.gid = os.getgid()

        if args.env:
            with open(args.env) as f:
                process.env = [line.rstrip() for line in f]

        if len(pargs) > idx:
            process.cmd = [wrkld] + pargs[idx].split()
        else:
            process.cmd = [wrkld]

        if len(inputs) > idx:
            process.input = inputs[idx]
        if len(outputs) > idx:
            process.output = outputs[idx]
        if len(errouts) > idx:
            process.errout = errouts[idx]

        multiprocesses.append(process)
        idx += 1

    if args.smt:
        cpu_type = ObjectList.cpu_list.get(args.cpu_type)
        assert ObjectList.is_o3_cpu(cpu_type), "SMT requires an O3CPU"
        return multiprocesses, idx
    else:
        return multiprocesses, 1


parser = argparse.ArgumentParser()
Options.addCommonOptions(parser)
Options.addSEOptions(parser)

args = parser.parse_args()

multiprocesses = []
numThreads = 1

if args.cmd:
    multiprocesses, numThreads = get_processes(args)
else:
    print("No workload specified. Exiting!\n", file=sys.stderr)
    sys.exit(1)

#  MinorCPU is an in-order CPU model with four fixed pipeline stages:
#
#  Fetch1 - fetches lines from memory
#  Fetch2 - decomposes lines into macro-op instructions
#  Decode - decomposes macro-ops into micro-ops
#  Execute - executes those micro-ops

np = 1
mp0_path = multiprocesses[0].executable
system = System(
    cpu=X86MinorCPU(cpu_id=0),  # Initialize cpu type
    mem_mode="timing",
    mem_ranges=[AddrRange(args.mem_size)],
    cache_line_size=args.cacheline_size,
)

FutureClass = None
system.membus = SystemXBar()
system.system_port = system.membus.cpu_side_ports

# Create a top-level voltage and clk domain
system.voltage_domain = VoltageDomain(voltage=args.sys_voltage)
system.clk_domain = SrcClockDomain(
    clock=args.sys_clock, voltage_domain=system.voltage_domain
)

# Create a CPU voltage and clk domain
system.cpu_voltage_domain = VoltageDomain()
system.cpu_clk_domain = SrcClockDomain(
    clock=args.cpu_clock, voltage_domain=system.cpu_voltage_domain
)
system.cpu.clk_domain = system.cpu_clk_domain
system.cpu.workload = multiprocesses[0]

# Add interrupt controller creation:
system.cpu.createInterruptController()

# Connect interrupts to memory bus:
system.cpu.interrupts[0].pio = system.membus.mem_side_ports
system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

# Example to change superscalar width. These are NOT the only parameters.
# width = 4
# system.cpu.decodeInputWidth=width
# system.cpu.executeInputWidth=width
# system.cpu.executeCommitLimit=width
# system.cpu.executeIssueLimit=width


##Add mem config
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports

# Create L1 Instruction Cache
class L1ICache(Cache):
    size = "16kB"
    assoc = 2
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20
    replacement_policy = RandomRP()


# Create L1 Data Cache
class L1DCache(Cache):
    size = "16kB"
    assoc = 2
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20
    replacement_policy = RandomRP()


# Create L2 Cache
class L2Cache(Cache):
    size = "256kB"
    assoc = 8
    tag_latency = 20
    data_latency = 20
    response_latency = 20
    mshrs = 20
    tgts_per_mshr = 12


# Set up cache hierarchy directly
system.cpu.icache = L1ICache()
system.cpu.dcache = L1DCache()
system.l2cache = L2Cache()

system.cpu.icache_port = system.cpu.icache.cpu_side
system.cpu.dcache_port = system.cpu.dcache.cpu_side

# Create buses
system.l2bus = L2XBar()

# Connect L1 caches to L2 bus
system.cpu.icache.mem_side = system.l2bus.cpu_side_ports
system.cpu.dcache.mem_side = system.l2bus.cpu_side_ports

# Connect L2 cache
system.l2cache.cpu_side = system.l2bus.mem_side_ports
system.l2cache.mem_side = system.membus.cpu_side_ports

if args.simpoint_profile:
    system.cpu.addSimPointProbe(args.simpoint_interval)

if args.checker:
    system.cpu.addCheckerCpu()

if args.bp_type:
    bpClass = ObjectList.bp_list.get(args.bp_type)
    system.cpu.branchPred = bpClass()

if args.indirect_bp_type:
    indirectBPClass = ObjectList.indirect_bp_list.get(args.indirect_bp_type)
    system.cpu.branchPred.indirectBranchPred = indirectBPClass()


system.cpu.createThreads()

system.workload = SEWorkload.init_compatible(mp0_path)

if args.wait_gdb:
    system.workload.wait_for_remote_gdb = True

root = Root(full_system=False, system=system)
Simulation.run(args, root, system, FutureClass)
