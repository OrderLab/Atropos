#!/usr/bin/python3

import os
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

from metrics_auxiliary import get_average_throughput, get_average_latency, get_p99_latency, get_recover_time


def analyze_baseline(case, log_dirs):
	avg_throughput_dict = []
	avg_latency_dict = []
	p99_latency_dict = []
	cancel_time_dict = []
	recover_time_dict = []
	for log_dir in log_dirs:
		baseline = log_dir.split('_')[0]
		year = log_dir.split('_')[1]
		month = log_dir.split('_')[2]
		day = log_dir.split('_')[3]

		throughput_log_df = pd.read_csv(
			os.path.join(
				os.getcwd(), "logs", "{}_{}_{}".format(year, month, day),
				log_dir, "{}_{}_throughput.csv".format(baseline, case)
			)
		)
		latency_log_df = pd.read_csv(
			os.path.join(
				os.getcwd(), "logs", "{}_{}_{}".format(year, month, day),
				log_dir, "{}_{}_latency.csv".format(baseline, case)
			)
		)

		avg_throughput_dict.append(
			get_average_throughput(throughput_log_df.values.squeeze())
		)
		avg_latency_dict.append(
			get_average_latency(latency_log_df.values.squeeze())
		)
		p99_latency_dict.append(
			get_p99_latency(latency_log_df.values.squeeze())
		)
		cancel_time_dict.append(
			0
		)
		recover_time_dict.append(
			get_recover_time(throughput_log_df.values.squeeze())
		)

	return avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict

def show_baseline_result(avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict):
	cases = list(avg_throughput_dict.keys())
	output_dict = {
		"Throughput (QPS)": [round(np.mean(avg_throughput_dict[case]), 2) for case in cases],
		"Mean Latency (ms)": [round(np.mean(avg_latency_dict[case]) / 1000000, 2) for case in cases],
		"P99 Latency (ms)": [round(np.mean(p99_latency_dict[case]) / 1000000, 2) for case in cases],
		"Cancel Time": [np.mean(cancel_time_dict[case]) for case in cases],
		"Recover Time": [np.mean(recover_time_dict[case]) for case in cases]
	}
	output_df = pd.DataFrame(output_dict, index=cases)
	output_df.to_markdown(buf=sys.stdout)
	print("")

if __name__ == "__main__":
	if len(sys.argv) < 3:
		print("Usage: ./log_analyzer.py case LOG_DIR1,LOG_DIR2,...")
	case = sys.argv[1]
	log_dirs = sys.argv[2].split(',')
	avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict = analyze_baseline(case, log_dirs)
	show_baseline_result({case: avg_throughput_dict}, {case: avg_latency_dict}, {case: p99_latency_dict}, {case: cancel_time_dict}, {case: recover_time_dict})

