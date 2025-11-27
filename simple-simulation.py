# simple_simulation.py

from m5.objects import *
from m5.util import addToPath
from os.path import join as joinpath
import m5

# Path to your compiled binary
binary = "tests/test-progs/hello/bin/x86/linux/hello"

# Create a system
system = System()

# Set the clock frequency and voltage
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "1GHz"
system.clk_domain.voltage_domain = VoltageDomain()

# Set up the memory controller
system.mem_mode = "timing"
system.mem_ranges = [AddrRange("512MB")]

# Define the CPU
# system.cpu = TimingSimpleCPU()
system.cpu = X86OOOCPU()
# Create the memory bus
system.membus = SystemXBar()

# Connect the CPU to the memory bus
system.cpu.icache_port = system.membus.slave
system.cpu.dcache_port = system.membus.slave

# Set up the memory controller using DDR3_1600_8x8
system.mem_ctrl = DDR3_1600_8x8()
system.mem_ctrl.range = system.mem_ranges[0]

# Instead of using `port`, now use `system.mem_ctrl` to connect the memory to membus
system.mem_ctrl.memory = system.membus.master

# Set up the process to run
process = Process()
process.cmd = [binary]
system.cpu.workload = process
system.cpu.createThreads()

# Connect the CPU to the IO bus
system.system_port = system.membus.slave

# Instantiate the system
root = Root(full_system=False, system=system)

# Start simulation
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()

print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")
