#!/usr/bin/python3

import os
import sys
import json
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

from metrics_auxiliary import get_cancel_time, get_recover_time, get_average_throughput, get_average_latency, get_p99_latency

experiment_modes = [
	"base_wo_predict", "base_w_predict", "moo_wo_predict", "moo_w_predict",
	"wo_cancel", "normal"
]

prefixes = [
	"abnormal_sensitivity", "interval_sensitivity", "rally_benchmark",
	"solr_bench"
]

RALLY_BENCHMARK_METRICS = ["random-indexing", "knn-filtered-search-multiple-client"]

SOLR_BENCH_SEARCH = ""
SOLR_BENCH_INDEX = ""

def get_log_dir_prefix(log_dir):
	for prefix in prefixes:
		if log_dir.startswith(prefix):
			return prefix


def remove_prefix(string, prefix):
	return string[len(prefix):] if string.startswith(prefix) else string


def remove_suffix(string, suffix):
	return string[:-len(suffix)] if string.endswith(suffix) else string


def get_log_files_with_mode(log_files, mode):
	log_files_with_mode = []
	for log_file in log_files:
		if mode in log_file:
			log_files_with_mode.append(log_file)
	return log_files_with_mode


def get_exp_settings(log_files, mode):
	exp_settings = []
	for log_file in log_files:
		case = log_file.split('_')[0]
		exp_setting = remove_suffix(
			remove_suffix(
				remove_suffix(
					remove_prefix(log_file, "{}_{}_".format(case, mode)),
					"_throughput.csv"
				), "_latency.csv"
			), ".csv"
		)
		if exp_setting not in exp_settings:
			exp_settings.append(exp_setting)
	return exp_settings


def analyze_sensitivity(log_dirs):
	avg_throughput_dict = {mode: {} for mode in experiment_modes}
	avg_latency_dict = {mode: {} for mode in experiment_modes}
	p99_latency_dict = {mode: {} for mode in experiment_modes}
	cancel_time_dict = {mode: {} for mode in experiment_modes}
	recover_time_dict = {mode: {} for mode in experiment_modes}
	for log_dir in log_dirs:
		microbenchmark = get_log_dir_prefix(log_dir)
		log_dir_wo_bench = remove_prefix(log_dir, "{}_".format(microbenchmark))
		year = log_dir_wo_bench.split('_')[0]
		month = log_dir_wo_bench.split('_')[1]
		day = log_dir_wo_bench.split('_')[2]

		log_dir_abs = os.path.join(
			os.getcwd(), "logs", "{}_{}_{}".format(year, month, day), log_dir
		)
		log_files = os.listdir(log_dir_abs)
		if len(log_files) == 0:
			exit()
		case = log_files[0].split('_')[0]

		for mode in experiment_modes:
			log_files_with_mode = get_log_files_with_mode(log_files, mode)
			lib_log_df = None
			throughput_log_df = None
			latency_log_df = None
			exp_settings = get_exp_settings(log_files_with_mode, mode)

			for exp_setting in exp_settings:
				avg_throughput_dict[mode].setdefault(exp_setting, [])
				avg_latency_dict[mode].setdefault(exp_setting, [])
				p99_latency_dict[mode].setdefault(exp_setting, [])
				cancel_time_dict[mode].setdefault(exp_setting, [])
				recover_time_dict[mode].setdefault(exp_setting, [])

				lib_log_df = pd.read_csv(
					os.path.join(
						log_dir_abs,
						"{}_{}_{}.csv".format(case, mode, exp_setting)
					),
					dtype={
						"Cancel": str,
						"Recover": str
					}
				)
				throughput_log_df = pd.read_csv(
					os.path.join(
						log_dir_abs, "{}_{}_{}_throughput.csv".format(
							case, mode, exp_setting
						)
					)
				)
				latency_log_df = pd.read_csv(
					os.path.join(
						log_dir_abs,
						"{}_{}_{}_latency.csv".format(case, mode, exp_setting)
					)
				)

				lib_log_list = []
				for index, row in lib_log_df.iterrows():
					# if row["Times"] == 1:
					#     continue
					lib_log_list.append(row.values)

				avg_throughput_dict[mode][exp_setting].append(
					get_average_throughput(throughput_log_df.values.squeeze())
				)
				avg_latency_dict[mode][exp_setting].append(
					get_average_latency(latency_log_df.values.squeeze())
				)
				p99_latency_dict[mode][exp_setting].append(
					get_p99_latency(latency_log_df.values.squeeze())
				)
				cancel_time_dict[mode][exp_setting].append(
					0 if mode == "wo_cancel" else get_cancel_time(lib_log_list)
				)
				recover_time_dict[mode][exp_setting].append(
					get_recover_time(throughput_log_df.values.squeeze())
				)
	return avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict


def show_sensitivity_result(
	avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict,
	recover_time_dict
):
	exp_settings = list(avg_throughput_dict[experiment_modes[0]].keys())
	output_dict = {
		"Settings": [exp_setting for mode in experiment_modes for exp_setting in exp_settings],
		"Throughput (QPS)": [round(np.mean(avg_throughput_dict[mode][exp_setting]), 2) for mode in experiment_modes for exp_setting in exp_settings],
		"Mean Latency (ms)": [round(np.mean(avg_latency_dict[mode][exp_setting]) / 1000000, 2) for mode in experiment_modes for exp_setting in exp_settings],
		"P99 Latency (ms)": [round(np.mean(p99_latency_dict[mode][exp_setting]) / 1000000, 2) for mode in experiment_modes for exp_setting in exp_settings],
		"Cancel Time": [np.mean(cancel_time_dict[mode][exp_setting]) for mode in experiment_modes for exp_setting in exp_settings],
		"Recover Time": [np.mean(recover_time_dict[mode][exp_setting]) for mode in experiment_modes for exp_setting in exp_settings]
	}
	output_df = pd.DataFrame(output_dict, index=[mode for mode in experiment_modes for exp_setting in exp_settings])
	output_df.to_markdown(buf=sys.stdout)
	print("")


def analyze_overhead(client_num, log_dirs):
	avg_throughput_dict = {"true": {}, "false": {}}
	p99_latency_dict = {"true": {}, "false": {}}
	for log_dir in log_dirs:
		microbenchmark = get_log_dir_prefix(log_dir)
		log_dir_wo_bench = remove_prefix(log_dir, "{}_".format(microbenchmark))
		year = log_dir_wo_bench.split('_')[0]
		month = log_dir_wo_bench.split('_')[1]
		day = log_dir_wo_bench.split('_')[2]
		log_dir_abs = os.path.join(
			os.getcwd(), "logs", "{}_{}_{}".format(year, month, day), log_dir
		)
		if microbenchmark == "rally_benchmark":
			log_files = os.listdir(log_dir_abs)
			for log_file in log_files:
				if '-' not in log_file:
					continue
				enable_autocancel = log_file.split('-')[1]
				benchmark_result_df = pd.read_csv(
					os.path.join(log_dir_abs, log_file)
				)
				benchmark_result_list = benchmark_result_df.values.tolist()
				for benchmark_result in benchmark_result_list:
					if benchmark_result[0] == "99th percentile latency" and benchmark_result[1] in RALLY_BENCHMARK_METRICS:
						p99_latency_dict[enable_autocancel].setdefault(benchmark_result[1], [])
						p99_latency_dict[enable_autocancel][
							benchmark_result[1]].append(
								(benchmark_result[2], benchmark_result[3])
							)
					if benchmark_result[0] == "Mean Throughput" and benchmark_result[1] in RALLY_BENCHMARK_METRICS:
						avg_throughput_dict[enable_autocancel].setdefault(benchmark_result[1], [])
						avg_throughput_dict[enable_autocancel][
							benchmark_result[1]].append(
								(benchmark_result[2], benchmark_result[3])
							)
		elif microbenchmark == "solr_bench":
			log_files = os.listdir(log_dir_abs)
			for log_file in log_files:
				if '_' not in log_file:
					continue
				enable_autocancel = log_file.split('_')[1]
				avg_throughput_dict[enable_autocancel].setdefault("Query", [])
				avg_throughput_dict[enable_autocancel].setdefault("Index", [])
				p99_latency_dict[enable_autocancel].setdefault("Query", [])
				p99_latency_dict[enable_autocancel].setdefault("Index", [])
				benchmark_result_file = open(
					os.path.join(log_dir_abs, log_file)
				)
				benchmark_result_dict = json.load(benchmark_result_file)
				benchmark_result_file.close()
				p99_latency_dict[enable_autocancel]["Query"].append(
					(benchmark_result_dict["task2"][0]["timings"][0]["99th"], "ms")
				)
				total_queries = int(
					benchmark_result_dict["task2"][0]["timings"][0]
					["total-queries"]
				)
				total_time = int(
					benchmark_result_dict["task2"][0]["timings"][0]
					["total-time"]
				) / 1000
				avg_throughput_dict[enable_autocancel]["Query"].append(
					(total_queries / total_time, "qps")
				)

				p99_latency_dict[enable_autocancel]["Index"].append(
					(benchmark_result_dict["task1"][0]["timings"]["cloud"][0]["99th"], "ms")
				)
				total_queries = int(
					benchmark_result_dict["task1"][0]["timings"]["cloud"][0]
					["total-queries"]
				)
				total_time = int(
					benchmark_result_dict["task1"][0]["timings"]["cloud"][0]
					["total-time"]
				) / 1000
				avg_throughput_dict[enable_autocancel]["Index"].append(
					(total_queries / total_time, "qps")
				)

	return avg_throughput_dict, p99_latency_dict, None, None, None


def show_overhead_result(avg_throughput_dict, p99_latency_dict, placeholder_0, placeholder_1, placeholder_2):
	enable_list = ["true", "false"]
	client_num_list = list(avg_throughput_dict.keys())
	metrics_list = list(avg_throughput_dict[client_num_list[0]][enable_list[0]].keys())
	output_dict = {
		"Metrics": [metrics for client_num in client_num_list for metrics in metrics_list for enable in enable_list],
		"Enable": [enable for client_num in client_num_list for metrics in metrics_list for enable in enable_list],
		"Throughput (QPS)": [round(np.mean([x[0] for x in avg_throughput_dict[client_num][enable][metrics]]), 2) for client_num in client_num_list for metrics in metrics_list for enable in enable_list],
		"P99 Latency (ms)": [round(np.mean([x[0] for x in p99_latency_dict[client_num][enable][metrics]]), 2) for client_num in client_num_list for metrics in metrics_list for enable in enable_list]
	}
	output_df = pd.DataFrame(output_dict, index=[client_num for client_num in client_num_list for metrics in metrics_list for enable in enable_list])
	output_df.to_markdown(buf=sys.stdout)
	print("")


if __name__ == "__main__":
	if len(sys.argv) == 2:
		log_dirs = sys.argv[1].split(',')
		if len(log_dirs) == 0:
			exit()
	
		avg_throughput_dict, avg_latency_dict, p99_latency_dict, cancel_time_dict, recover_time_dict = analyze_sensitivity(
			log_dirs
		)
		show_sensitivity_result(
			avg_throughput_dict, avg_latency_dict, p99_latency_dict,
			cancel_time_dict, recover_time_dict
		)
	elif len(sys.argv) == 3:
		log_dirs = sys.argv[2].split(',')
		if len(log_dirs) == 0:
			exit()
	
		client_num = sys.argv[1]
		avg_throughput_dict, p99_latency_dict, placeholder_0, placeholder_1, placeholder_2 = analyze_overhead(client_num, log_dirs)
		show_overhead_result({client_num: avg_throughput_dict}, {client_num: p99_latency_dict}, placeholder_0, placeholder_1, placeholder_2)
	else:
		print("Usage: ./log_analyzer.py (case) LOG_DIR1,LOG_DIR2,...")
