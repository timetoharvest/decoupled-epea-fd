import platform
import re

from lab.environments import SlurmEnvironment


class BWUniEnvironment(SlurmEnvironment):
    """Environment for the bwUniCluster in Baden-Württemberg.

    This is the environment most students should use: the registration
    described in the README only grants access to the bwUniCluster, so unless
    you were explicitly told otherwise, this is the cluster you are running on.
    """

    DEFAULT_PARTITION = "cpu"
    DEFAULT_QOS = "normal"
    DEFAULT_TIME_LIMIT_PER_TASK = "12:00:00"
    # 64 cores on cpu nodes, 236G usable memory
    DEFAULT_MEMORY_PER_CPU = "3600M"
    # See slurm.conf
    MAX_TASKS = 1000

    @classmethod
    def is_present(cls):
        node = platform.node()
        # Node names look like uc3n<digits>. Which login node you end up on
        # varies (uc3n990, uc3n991, ...), so we deliberately match them all
        # instead of a single host. Check with "hostname" if lab does not
        # detect the cluster.
        return bool(re.match(r"uc3n\d+", node))

class HelixEnvironment(SlurmEnvironment):
    """Environment for the Helix cluster in Baden-Württemberg.

    Only relevant if you have a Helix account; use BWUniEnvironment otherwise.
    """

    DEFAULT_PARTITION = "cpu-single"
    DEFAULT_QOS = "normal"
    DEFAULT_TIME_LIMIT_PER_TASK = "12:00:00"
    # 64 cores on cpu nodes, 236G usable memory
    DEFAULT_MEMORY_PER_CPU = "3600M"
    # See slurm.conf
    MAX_TASKS = 1000

    @classmethod
    def is_present(cls):
        node = platform.node()
        return bool(re.match(r"o05i15", node))
