"""
    run_suite.py
    By: DrkWithT
"""

import os
import subprocess

DERKJS_TEST_SUITE_DIR = os.path.relpath('./test_suite')
DERKJS_TEST_SUITE_GROUPS = ['basic'] # TODO add 'objects' and 'builtins'
DERKJS_TEST_PROCESS_COUNT = 2;

def get_test_names(test_suite_path: str = DERKJS_TEST_SUITE_DIR, folders: list[str] = DERKJS_TEST_SUITE_GROUPS) -> list[str]:
    all_test_names = []

    for folder_name in folders:
        current_folder_prefix = f'{test_suite_path}/{folder_name}'
        current_folder_entries = os.listdir(current_folder_prefix)
        all_test_names += [
            f'{current_folder_prefix}/{test_case_filename}'
            for test_case_filename
            in current_folder_entries
        ]
    
    return all_test_names

def run_tests_by_n(test_file_paths: list[str], worker_count: int = DERKJS_TEST_PROCESS_COUNT):
    total_tests = len(test_file_paths)
    total_passed = 0

    while test_file_paths:
        batch = [(
            test_path,
            f'./build/derkjs_tco -r {test_path}'
        ) for test_path in test_file_paths[:worker_count]]
        test_file_paths = test_file_paths[worker_count:]

        batched_procs = [subprocess.Popen(
            named_test[1],
            shell=True
        ) for named_test in batch]

        for batch_test_id, test_cmd in enumerate(batched_procs):
            if test_cmd.wait() == 0:
                print(f'Test \x1b[1;33m{batch[batch_test_id][0]}\x1b[0m:  \x1b[1;32mPASS\x1b[0m\n')
                total_passed += 1
            else:
                print(f'Test \x1b[1;33m{batch[batch_test_id][0]}\x1b[0m:  \x1b[1;31mFAIL\x1b[0m\n')

    return (total_passed, total_tests - total_passed, total_tests)   

if __name__ == '__main__':
    pass_count, fail_count, test_count = run_tests_by_n(
        get_test_names()
    )

    print(f'\nTEST REPORT:\n\x1b[1;34mPASSED:\x1b[0m {pass_count}/{test_count}\n\x1b[1;34mFAILED:\x1b[0m {fail_count}/{test_count}\n')
