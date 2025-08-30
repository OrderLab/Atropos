import os
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

from metrics_auxiliary import get_cancel_time, get_recover_time, get_average_throughput, get_average_latency, get_p99_latency, draw_throught, get_average_wo_abnormal

experiment_modes = [
	"base_wo_predict", "base_w_predict", "moo_wo_predict", "moo_w_predict",
	"wo_cancel", "normal"
]

def analyze_case(log_dirs):
	avg_throughput_dict = {mode: [] for mode in experiment_modes}
	avg_latency_dict = {mode: [] for mode in experiment_modes}
	p99_latency_dict = {mode: [] for mode in experiment_modes}
	cancel_time_dict = {mode: [] for mode in experiment_modes}
	recover_time_dict = {mode: [] for mode in experiment_modes}
	for log_dir in log_dirs:
		year = log_dir.split('_')[1]
		month = log_dir.split('_')[2]
		day = log_dir.split('_')[3]

		for mode in experiment_modes:
			lib_log_df = pd.read_csv(
				os.path.join(
					os.getcwd(), "logs", "{}_{}_{}".format(year, month, day),
					log_dir, "{}.csv".format(mode)
				),
				dtype={
					'Cancel': str,
					"Recover": str
				}
			)
			throughput_log_df = pd.read_csv(
				os.path.join(
					os.getcwd(), "logs", "{}_{}_{}".format(year, month, day),
					log_dir, "{}_throughput.csv".format(mode)
				)
			)
			latency_log_df = pd.read_csv(
				os.path.join(
					os.getcwd(), "logs", "{}_{}_{}".format(year, month, day),
					log_dir, "{}_latency.csv".format(mode)
				)
			)

			lib_log_list = []
			for index, row in lib_log_df.iterrows():
				# if row["Times"] == 1:
				#     continue
				lib_log_list.append(row.values)

			avg_throughput_dict[mode].append(
				get_average_throughput(throughput_log_df.values.squeeze())
			)
			avg_latency_dict[mode].append(
				get_average_latency(latency_log_df.values.squeeze())
			)
			p99_latency_dict[mode].append(
				get_p99_latency(latency_log_df.values.squeeze())
			)
			cancel_time_dict[mode].append(
				0 if mode == "wo_cancel" else get_cancel_time(lib_log_list)
			)
			recover_time_dict[mode].append(get_recover_time(throughput_log_df.values.squeeze()))
			draw_throught(
				lib_log_list,
				os.path.join(
					os.getcwd(), "logs", "{}_{}_{}".format(year, month, day),
					log_dir, "{}.jpg".format(mode)
				)
			)

	return avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict

def show_case_result(avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict):
	output_dict = {
		"Throughput (QPS)": [round(get_average_wo_abnormal(avg_throughput_dict[mode], 1), 2) for mode in experiment_modes],
		"Mean Latency (ms)": [round(get_average_wo_abnormal(avg_latency_dict[mode], 1) / 1000000, 2) for mode in experiment_modes],
		"P99 Latency (ms)": [round(get_average_wo_abnormal(p99_latency_dict[mode], 1) / 1000000, 2) for mode in experiment_modes],
		"Cancel Time": [round(get_average_wo_abnormal(cancel_time_dict[mode], 1), 2) for mode in experiment_modes],
		"Recover Time": [round(get_average_wo_abnormal(recover_time_dict[mode], 1), 2) for mode in experiment_modes]
	}
	output_df = pd.DataFrame(output_dict, index=experiment_modes)
	output_df.to_markdown(buf=sys.stdout)
	print("")

if __name__ == "__main__":
	if len(sys.argv) < 2:
		print("Usage: ./log_analyzer.py LOG_DIR1,LOG_DIR2,...")

	log_dirs = sys.argv[1].split(',')
	avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict = analyze_case(log_dirs)
	show_case_result(avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict)

