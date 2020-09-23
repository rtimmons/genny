"""
Parse cedar-csv output into legacy "perf.json" report file format.
"""
import argparse
import json
import logging
import sys
import os

from gennylib.parsers.csv2 import CSV2, IntermediateCSVColumns


def build_parser():
    parser = argparse.ArgumentParser(
        description="Convert cedar-csv output into legacy perf.json report file format"
    )
    return parser


class _LegacyReportIntermediateFormat(object):
    def __init__(self):
        self.duration_sum = 0
        self.n = 0
        self.threads = set()
        self.started = sys.maxsize
        self.ended = 0
        self.ops_per_ns = 0

    def finalize(self):
        self.ops_per_ns = self.n / (self.ended - self.started)
        return self

    def _asdict(self):
        # Name derives from the _asdict method of colllections.namedtuple
        return {
            "started": self.started,
            "ended": self.ended,
            "ops_per_ns": self.ops_per_ns,
            "threads": self.threads,
        }


def _translate_to_perf_json(timers):
    out = []
    for name, timer in timers.items():
        out.append(
            {
                "name": name,
                "workload": name,
                "start": timer["started"] / 100000,
                "end": timer["ended"] / 100000,
                "results": {
                    len(timer["threads"]): {
                        "ops_per_sec": timer["ops_per_ns"] * 1e9,
                        "ops_per_sec_values": [timer["ops_per_ns"] * 1e9],
                    }
                },
            }
        )
    return {"results": out}


def run(input_csv_file, perf_json_file):
    timers = {}

    # There may not be a CSV file and DSI wouldn't
    # know without reading the workload yml.
    if not os.path.exists(input_csv_file):
        return

    my_csv2 = CSV2(input_csv_file)

    iterations = 0
    with my_csv2.data_reader() as data_reader:
        metric_name = None
        report = None

        for line, actor in data_reader:
            cur_metric_name = "{}-{}".format(actor, line[IntermediateCSVColumns.OPERATION])

            if cur_metric_name != metric_name:
                if report:
                    # pylint: disable=protected-access
                    timers[metric_name] = report.finalize()._asdict()
                metric_name = cur_metric_name
                report = _LegacyReportIntermediateFormat()

            end_metric_time = line[IntermediateCSVColumns.TS]
            duration = line[IntermediateCSVColumns.DURATION]
            end_system_time = my_csv2.metric_to_system_time_ns(end_metric_time)
            start_system_time = end_system_time - duration

            report.threads.add(line[IntermediateCSVColumns.THREAD])
            report.started = min(report.started, start_system_time)
            report.ended = max(report.ended, end_system_time)
            report.duration_sum += duration
            report.n += 1

            iterations = iterations + 1
            if iterations % 1e6 == 0:
                logging.info("Processed %d metrics lines", iterations)

        if report:
            # pylint: disable=protected-access
            timers[metric_name] = report.finalize()._asdict()

    result = _translate_to_perf_json(timers)

    os.makedirs(os.path.dirname(perf_json_file), exist_ok=True)
    with open(perf_json_file, "w") as f:
        json.dump(result, f)


def main__legacy_report(argv=None):
    """
    Entry point to convert cedar csv metrics format into legacy perf.json
    :return: None
    """
    if argv is None:
        argv = sys.argv[1:]
    logging.basicConfig(level=logging.INFO)
    parser = build_parser()
    parser.parse_args(argv)
    logging.info("Running legacy metrics report from cwd={}", os.getcwd())
    run(input_csv_file="./build/WorkloadOutput/GennyMetrics/genny-metrics.csv",
        perf_json_file="./build/WorkloadOutput/LegacyPerfJson/perf.json")
