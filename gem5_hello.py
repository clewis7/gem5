""" Gem5 script to execute a simple Hello World application.
This script uses the X86 ISA.
"""
import m5
from m5.objects import *
import os

# Create the system
system = System()

# Set basic clock domain
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "1GHz"
system.clk_domain.voltage_domain = VoltageDomain()

# Set up the system
system.mem_mode = "timing"
system.mem_ranges = [AddrRange("8192MB")]

# Create a simple CPU
system.cpu = TimingSimpleCPU()

# Create memory bus
system.membus = SystemXBar()

# Connect CPU to memory bus
system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports

# Create interrupt controller (required for X86)
system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.mem_side_ports
system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

# Create memory controller
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports

# Connect system port
system.system_port = system.membus.cpu_side_ports

# Set up the hello world binary
thispath = os.path.dirname(os.path.realpath(__file__))
binary = os.path.join(thispath, "tests/test-progs/hello/bin/x86/linux/hello")
system.workload = SEWorkload.init_compatible(binary)

# Create process
process = Process()
process.cmd = [binary]
system.cpu.workload = process
system.cpu.createThreads()

# Run simulation
root = Root(full_system=False, system=system)
m5.instantiate()
print("Beginning simulation!")
exit_event = m5.simulate()

# Print simulation results
print(f"Exiting @ tick {m5.curTick()} because {exit_event.getCause()}")

# Use ticks as cycles (1GHz = 1 tick per cycle)
total_cycles = m5.curTick()
print(f"Total cycles: {total_cycles}")

# Get memory statistics from stats file
from m5.stats import dump

# Dump all statistics to file
dump()
