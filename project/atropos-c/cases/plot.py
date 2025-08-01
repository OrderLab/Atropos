import matplotlib.pyplot as plt
import re

# Input data
with open("/users/gongxini/cases/postgres-case/wal_case/wlc-inter-2", "r") as file:
    sysbench_output = file.readlines()

# Parse the sysbench output
def parse_sysbench_output(output):
    time_points = []
    tps_values = []
    
    for line in output:
        match = re.match(r"\[\s*(\d+\.\ds)\s*\] thds: \d+ tps: ([\d\.]+)", line)
        if match:
            time_points.append(float(match.group(1).replace('s', '')))
            tps_values.append(float(match.group(2)))

    return time_points, tps_values

# Extract time points and TPS values
time_points, tps_values = parse_sysbench_output(sysbench_output)

# Plot the TPS values over time
plt.figure(figsize=(10, 6))
plt.plot(time_points, tps_values, marker='o', linestyle='-', label='TPS')
plt.title('TPS Over Time from Sysbench Output')
plt.xlabel('Time (seconds)')
plt.ylabel('Transactions Per Second (TPS)')
plt.grid(True)
plt.legend()
plt.tight_layout()

# Show the plot
plt.savefig("a.png")
