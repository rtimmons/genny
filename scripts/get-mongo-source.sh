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
set -o nounset

SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "${SCRIPTS_DIR}/.." && pwd)"
SOURCE_DIR="${ROOT_DIR}/build/mongo"
VENV_DIR="${ROOT_DIR}/build/venv"

git clone git@github.com:mongodb/mongo.git "${SOURCE_DIR}"

(
    cd "${SOURCE_DIR}"
    git checkout 6734c12d17dd4c0e2738a47feb7114221d6ba66d
)

virtualenv -p python2 "${VENV_DIR}"

export VIRTUAL_ENV_DISABLE_PROMPT="yes"
# shellcheck disable=SC1090
. "${VENV_DIR}/bin/activate"

python -m pip install -r "${SOURCE_DIR}/etc/pip/evgtest-requirements.txt"
