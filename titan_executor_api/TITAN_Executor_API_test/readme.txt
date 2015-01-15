###############################################################################
# Copyright (c) 2000-2014 Ericsson Telecom AB
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
###############################################################################
build.xml is generated.
The generated build.xml is modified manually:
  target TITAN_Executor_API_test is modified to fail in ant level if any testcase fails to make Jenkins show the failures
  failureproperty="test.failed" added to <junit fork="yes" printsummary="withOutAndErr" HERE>
  <fail if="test.failed" message="TITAN_Executor_API_test FAILED"/> added after </junit>

Steps to generate build.xml from Eclipse:
  1. Right click on TITAN_Executor_API -> Export...
  2. Select General/Ant Buildfiles
JUnit will be included in build.xml

Run JUnit tests from Ant build.xml:
ant TITAN_Executor_API_test
