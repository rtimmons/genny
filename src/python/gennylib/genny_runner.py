import sys
import os
import subprocess
from contextlib import contextmanager


def get_poplar_args():
    """
    Returns the argument list used to create the Poplar gRPC process.

    If we are in the root of the genny repo, use the local executable. 
    Otherwise we search the PATH.
    """
    curator = "curator"

    local_curator = "./bin/curator/curator"
    if os.path.exists(local_curator):
        curator = local_curator
    return [curator, "poplar", "grpc"]


@contextmanager
def poplar_grpc():
    poplar = subprocess.Popen(get_poplar_args())
    if poplar.poll() is not None:
        raise OSError("Failed to start Poplar.")

    try:
        yield poplar
    finally:
        try:
            poplar.terminate()
            exit_code = poplar.wait(timeout=10)
            if exit_code not in (0, -15):  # Termination or exit.
                raise OSError("Poplar exited with code: {code}.".format(code=exit_code))

        except:
            # If Poplar doesn't die then future runs can be broken.
            if poplar.poll() is None:
                poplar.kill()
            raise


def main_genny_runner():
    """
    Intended to be the main entry point for running Genny.
    """

    with poplar_grpc():
        res = subprocess.run("./src/genny/dist/bin/genny_core")
        res.check_returncode()


if __name__ == "__main__":
    main_genny_runner()
