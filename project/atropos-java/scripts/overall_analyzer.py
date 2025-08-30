#!/usr/bin/python3
import sys
import os

import pandas as pd
import numpy as np

from cases_analyzer import analyze_case, show_case_result
from microbenchmarks_analyzer import analyze_sensitivity, show_sensitivity_result, analyze_overhead, show_overhead_result
from baselines_analyzer import analyze_baseline, show_baseline_result

cases_names = ["c1", "c3", "c4", "c5", "c6", "c7"]
microbenchmark_names = ["abnormal_sensitivity_c1", "abnormal_sensitivity_c3", "abnormal_sensitivity_c4", "abnormal_sensitivity_c6", "abnormal_sensitivity_c7"]
overhead_names = ["rally_benchmark", "solr_bench"]
baseline_names = ["parties", "psp", "psandbox"]

if __name__ == "__main__":
	exp_name = sys.argv[1]
	exp_type = None
	analyze_func = None
	show_result_func = None
	if exp_name in cases_names:
		exp_type = "cases"
		analyze_func = analyze_case
		show_result_func = show_case_result
	elif exp_name in microbenchmark_names:
		exp_type = "microbenchmark"
		analyze_func = analyze_sensitivity
		show_result_func = show_sensitivity_result
	elif exp_name in overhead_names:
		exp_type = "microbenchmark"
		def analyze_overheads(file_names_list):
			avg_throughput_dict, avg_latency_dict = {}, {}
			for index, client_num in enumerate(file_names_list[0]):
				avg_throughput_list, avg_latency_list, placeholder_0, placeholder_1, placeholder_2 = analyze_overhead(
					client_num, [log_dir[index] for log_dir in file_names_list[1:]]
				)
				avg_throughput_dict[client_num] = avg_throughput_list
				avg_latency_dict[client_num] = avg_latency_list
			return avg_throughput_dict, avg_latency_dict, None, None, None

		analyze_func = analyze_overheads
		show_result_func = show_overhead_result
	elif exp_name in baseline_names:
		exp_type = "baseline"
		def analyze_baselines(file_names_list):
			avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict = {}, {}, {}, {}, {}
			for index, case in enumerate(file_names_list[0]):
				avg_throughput_list, avg_latency_list, p99_latency_list, cancel_time_list, recover_time_list = analyze_baseline(
					case, [log_dir[index] for log_dir in file_names_list[1:]]
				)
				avg_throughput_dict[case] = avg_throughput_list
				avg_latency_dict[case] = avg_latency_list
				p99_latency_dict[case] = p99_latency_list
				cancel_time_dict[case] =cancel_time_list 
				recover_time_dict[case] = recover_time_list
			return avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict

		analyze_func = analyze_baselines
		show_result_func = show_baseline_result
	else:
		print("Error arg")
		print("\tChoices: {}".format(cases_names + microbenchmark_names + overhead_names + baseline_names))
		exit()

	file_names_df = pd.read_csv(os.path.join(os.getcwd(), "results", exp_type, "{}.csv".format(exp_name)), header=None)
	file_names_list = list(np.array(file_names_df.values.tolist()).squeeze())
	avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict = analyze_func(file_names_list)

	show_result_func(avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict)

