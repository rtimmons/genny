#!/bin/bash

# Copyright 2019-present MongoDB Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -o errexit
set -o pipefail
# set -o nounset # the "activate" script has an unbound variable

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

VENV_DIR="${VENV_DIR:-${BUILD_DIR}/venv}"
MONGO_DIR="${MONGO_DIR:-${BUILD_DIR}/mongo}"
RESMOKE_SUITE="${RESMOKE_SUITE:-genny_standalone.yml}"
SENTINEL_XML="${BUILD_DIR}/sentinel.junit.xml"

if [[ -z $VIRTUAL_ENV ]]; then
    export VIRTUAL_ENV_DISABLE_PROMPT="yes"
    # shellcheck disable=SC1090
    . "${VENV_DIR}/bin/activate"
fi

# Move to the root dir because of how resmoke paths
cd "${ROOT_DIR}"

# We rely on catch2 to report test failures, but it doesn't always do so.
# See https://github.com/catchorg/Catch2/issues/1210
# As a workaround, we generate a dummy report with a failed test that is deleted if resmoke
# succeeds.

cat << EOF >> "${SENTINEL_XML}"
<?xml version="1.0" encoding="UTF-8"?>
<testsuites>
 <testsuite name="resmoke_failure_sentinel" errors="0" failures="1" tests="1" hostname="tbd" time="1.0" timestamp="2019-01-01T00:00:00Z">
  <testcase classname="resmoke_failure_sentinel" name="Dummy testcase to signal that resmoke failed because a report may not be generated" time="1.0">
   <failure message="resmoke did not exit cleanly, see task log for detail" type="">
   </failure>
  </testcase>
  <system-out/>
  <system-err/>
 </testsuite>
</testsuites>
EOF

# Add the mongo dir to the end of the path so we can find mongo executables
export PATH+=":${MONGO_DIR}"
export PATH+=":${MONGO_DIR}/bin"

# The tests themselves do the reporting instead of using resmoke.
python2 "${MONGO_DIR}/buildscripts/resmoke.py" \
       --suite "${ROOT_DIR}/src/resmokeconfig/${RESMOKE_SUITE}" \
       --mongod mongod \
       --mongo mongo \
       --mongos mongos

# Remove the sentinel report if resmoke succeeds. This line won't be executed if
# resmoke fails because we've set errexit on this shell.
rm "${SENTINEL_XML}"
