
SHELL := /bin/bash

run-apps:
	echo "running Aquabolt" ;\
	cd Aquabolt ;\
	scons
	cd Aquabolt ;\
	./sim --gtest_filter=PIMBenchFixture.mul
	cd Aquabolt ;\
	./sim --gtest_filter=PIMBenchFixture.gemv
	cd Aquabolt ;\
	./sim --gtest_filter=PIMBenchFixture.add
clean:
	cd Aquabolt ;\
	scons -c
