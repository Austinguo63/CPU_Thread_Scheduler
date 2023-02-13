import os
import sys
import subprocess
import itertools
from timeit import repeat


mem_check_prefix = ("valgrind", "--leak-check=full", "--errors-for-leak-kinds=all", "--error-exitcode=1")


def main():
    num_tests = 0
    num_passed = 0
    num_failed = 0
    failed_tests = set()

    mem_check = False
    repeats = 1

    if len(sys.argv) > 1:
        print(sys.argv[1])
        if sys.argv[1].lower() == "memcheck":
            mem_check = True
        elif sys.argv[1].lower() == "standard":
            mem_check = False
        else:
            print(f"Unknown option {sys.argv[1]}")
            exit(-1)
    
    if len(sys.argv) > 2:
        try:
            repeats = int(sys.argv[2])
            if repeats < 0:
                print("Repeat can not be negative!")
                exit(-1)
        except ValueError:
            print(f"Can not parse repeat value {sys.argv[2]} as integer!")
            exit(-1)

    print(mem_check)
    print(repeats)
    output_test_strings = tuple(sorted(os.listdir("sample_output"))) * repeats
    for output_test_string in output_test_strings:
        num_tests += 1
        _, scheduler_type, input_name = output_test_string.split("-")
        print(f"Running test: {scheduler_type=},{input_name=}")
        arguments = ("./proj1", scheduler_type, f"sample_input/{input_name}")
        timeout = 20.0 if mem_check else 1.0
        if mem_check:
            arguments = mem_check_prefix + arguments
        try:
            process = subprocess.run(arguments, capture_output=True, timeout=timeout)
            if process.returncode != 0:
                if mem_check:
                    print("Memory Errors Found")
                    print(process.stderr.decode("utf8"))
                else:
                    print("Program Crushed!")
                    print(process.stdout.decode("utf8"))
                    print(process.stderr.decode("utf8"))
                num_failed += 1
                failed_tests.add((scheduler_type, input_name))
            else:
                with open(f"sample_output/{output_test_string}") as expected_file:
                    with open(f"output/{output_test_string}") as actual_file:
                        expected_lines = tuple(sorted(map(lambda x: x.strip(), expected_file.readlines())))
                        actual_lines = tuple(sorted(map(lambda x: x.strip(), actual_file.readlines())))
                        if (expected_lines != actual_lines):
                            print("output MISMATCH")
                            print("Expecting: \t \t \t Actual:")
                            print("\n".join(map("\t".join, itertools.zip_longest(expected_lines, actual_lines, fillvalue=""))))

                            num_failed += 1
                            failed_tests.add((scheduler_type, input_name))
                        else :
                            print("Test PASSED")
                            num_passed += 1
        except subprocess.TimeoutExpired as e:
            print("Program Timed Out! Possible Deadlock")
            if e.output is not None:
                print(e.output.decode("utf8"))
            num_failed += 1
            failed_tests.add((scheduler_type, input_name))
    
    print(f"RUN {num_tests} TESTS:")
    print(f"{num_failed} FAILED, {num_passed} PASSED")
    if (num_failed == 0):
        print("ALL TESTS PASSED")
        exit(0)
    else:
        print("FAILED TESTS:")
        for scheduler_type, input_name in failed_tests:
            print(f"Scheduler {scheduler_type}, {input_name}")
        exit(-1)


if __name__ == "__main__":
    main()
