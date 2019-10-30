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

from setuptools import setup

setup(name='genny',
      version='1.0',
      packages=[
          'genny',
          'genny.parsers',
          'third_party'
      ],
      install_requires=[
          'nose==1.3.7',
          'yapf==0.24.0',
          'pymongo==3.7.2',
          'PyYAML==5.1',
          'requests==2.21.0',
          'yamllint==1.15.0',
          'shrub.py==0.2.3'
      ],
      setup_requires=[
          'nose==1.3.7'
      ],
      entry_points={
          'console_scripts': [
              'genny-metrics-report = genny.cedar_report:main__cedar_report',
              'genny-metrics-legacy-report = genny.legacy_report:main__legacy_report',
              'lint-yaml = genny.yaml_linter:main',
              'genny-auto-tasks = genny.genny_auto_tasks:main'
          ]
      },
      )
