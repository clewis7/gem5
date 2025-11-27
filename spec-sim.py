# Import the gem5 library
import m5
from m5.objects import *

# Create the system we are going to simulate
system = System()

# Set the clock frequency of the system (and all of its children)
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "2GHz"
system.clk_domain.voltage_domain = VoltageDomain()

# Set the memory mode
system.mem_mode = "timing"  # Use timing accesses
system.mem_ranges = [
    AddrRange("8192MB")
]  # Increase system memory to match DRAM

system.cpu = X86O3CPU()  # Use out-of-order CPU model

# Create a memory bus, a system crossbar
system.membus = SystemXBar()

# Hook the CPU ports up to the membus
system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports

# Create the interrupt controller for the CPU and connect to the membus
system.cpu.createInterruptController()

# X86-specific interrupt connections
system.cpu.interrupts[0].pio = system.membus.mem_side_ports
system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

# Create a memory controller and connect it to the membus
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[
    0
]  # Adjust DRAM size to match system memory
system.mem_ctrl.port = system.membus.mem_side_ports

# Connect the system port to the membus
system.system_port = system.membus.cpu_side_ports

# Set the SPEC2017 benchmark binary path and command options
binary = "/home/ec2-user/spec2017/benchspec/CPU/600.perlbench_s/build/build_base_mytest-m64.0000/perlbench_s"
options = "-I /home/ec2-user/spec2017/benchspec/CPU/600.perlbench_s/run/run_base_refspeed_mytest-m64.0000/lib /home/ec2-user/spec2017/benchspec/CPU/600.perlbench_s/run/run_base_refspeed_mytest-m64.0000/checkspam.pl 2500 5 25 11 150 1 1 1 1"
# ../run_base_refspeed_mytest-m64.0000/perlbench_s_base.mytest-m64 -I./lib checkspam.pl 2500 5 25 11 150 1 1 1 1

# binary = "/home/ec2-user/spec2017/benchspec/CPU/619.lbm_s/build/build_base_mytest-m64.0000/lbm_s"
# options = "2000 reference.dat 0 0 /home/ec2-user/spec2017/benchspec/CPU/619.lbm_s/run/run_base_refspeed_mytest-m64.0000/200_200_260_ldc.of"

# binary = "/home/ec2-user/microbench/CF1/bench.X86"
# options = ""

# binary = "/home/ec2-user/spec2017/benchspec/CPU/602.gcc_s/build/build_base_mytest-m64.0000/sgcc"
# options = "/home/ec2-user/spec2017/benchspec/CPU/602.gcc_s/run/run_base_refspeed_mytest-m64.0000/gcc-pp.c -O5 -finline-limit=1000 -fselective-scheduling -fselective-scheduling2 -o gcc-pp.opts-O5_-finline-limit_1000_-fselective-scheduling_-fselective-scheduling2.s"
# Set the workload to the SPEC2017 binary
system.workload = SEWorkload.init_compatible(binary)

# Create a process for the SPEC2017 benchmark
process = Process()
process.cmd = [
    binary
] + options.split()  # Command to run the SPEC2017 binary with options

# Set the CPU to use the process as its workload and create thread contexts
system.cpu.workload = process
system.cpu.createThreads()

# Instantiate the root and system
root = Root(full_system=False, system=system)

# Instantiate all the objects we've created above
m5.instantiate()

# Begin simulation
print("Starting the simulation!")
exit_event = m5.simulate()

print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
