import os
import sys
import glob
import subprocess
from typing import NamedTuple, List, Optional

from shrub.config import Configuration


def _check_output(cwd, *args, **kwargs):
    old_cwd = os.getcwd()
    try:
        os.chdir(cwd)
        out = subprocess.check_output(*args, **kwargs)
    except subprocess.CalledProcessError as e:
        print(e.output, file=sys.stderr)
        raise e
    finally:
        os.chdir(old_cwd)

    if out.decode() == "":
        return []
    return out.decode().strip().split("\n")


class Repo:
    def __init__(self, repo_root: str):
        self.repo_root = repo_root
        self._modified_repo_files = None

    @staticmethod
    def _normalize_path(filename: str) -> str:
        return filename.split("workloads/", 1)[1]

    def _modified_workload_files(self):
        command = (
            "git diff --name-only --diff-filter=AMR "
            # TODO: don't use rtimmons/
            "$(git merge-base HEAD rtimmons/master) -- src/workloads/"
        )
        print(f"Command: {command}")
        lines = _check_output(self.repo_root, command, shell=True)
        return {os.path.join(self.repo_root, line)
                for line in lines
                if line.endswith(".yml")}

    def _all_workload_files(self):
        pattern = os.path.join(self.repo_root, "src", "workloads", "*", "*.yml")
        return {*glob.glob(pattern)}

    def all_workloads(self) -> List['Workload']:
        all_files = self._all_workload_files()
        modified = self._modified_workload_files()
        return [Workload(fpath, fpath in modified) for fpath in all_files]

    def modified_workloads(self) -> List['Workload']:
        return [workload
                for workload in self.all_workloads()
                if workload.is_modified]


class Runtime:
    def __init__(self, cwd: str):
        self.cwd = cwd

    def workload_setup(self):
        pass


class GeneratedTask(NamedTuple):
    mongodb_setup: str
    workload_path: str


class Workload:
    def __init__(self, file_path: str, is_modified: bool):
        self.file_path = file_path
        self.is_modified = is_modified

    def __repr__(self):
        return f"<{self.file_path},{self.is_modified}>"

    def all_tasks(self, runtime: Runtime) -> List[GeneratedTask]:
        return []

    def variant_tasks(self, runtime: Runtime) -> List[GeneratedTask]:
        return []


class TaskWriter:
    def write(self) -> Configuration:
        raise NotImplementedError()


class CLI:
    def __init__(self, cwd=None):
        self.cwd = cwd if cwd else os.getcwd()
        self.repo = Repo(cwd)
        self.runtime = Runtime(cwd)

    def main(self, argv=None):
        argv = argv if argv else sys.argv
        print(self.repo.all_workloads())

    def all_tasks(self) -> List[GeneratedTask]:
        """
        :return: All possible tasks
        """
        return [task
                for workload in self.repo.all_workloads()
                for task in workload.all_tasks(self.runtime)]

    def variant_tasks(self):
        """
        :return: Tasks to schedule given the current variant (runtime)
        """
        return [task
                for workload in self.repo.all_workloads()
                for task in workload.variant_tasks(self.runtime)]

    def patch_tasks(self):
        """
        :return: Tasks for modified workloads current variant (runtime)
        """
        return [task
                for workload in self.repo.modified_workloads()
                for task in workload.all_tasks(self.runtime)]


if __name__ == "__main__":
    cli = CLI("/Users/rtimmons/Projects/genny")
    cli.main()
