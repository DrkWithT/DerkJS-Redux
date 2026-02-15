# test262 harness for ES1, maybe up to ES5
# Setup:
# 1. Run: npm install -g esvu and then npm install -g eshost-cli
# 2. Set the ESHOST_PATH_DERKJS environment variable to the absolute executable's path in this project's build folder.
# 3. Run: eshost --add "derkjs" ch $ESHOST_PATH_DERKJS --args "-r"

# TODO:
# 1. Implement: Test262Error, assert.sameValue, assert.throws
# test262-harness -t 4 --timeout 4000 -r simple --host-type=derkjs --host-path ./build/derkjs_tco --host-args "-r" --test262-dir ./ecmascript_tests/test262/ --tempDir ./ecmascript_test262_logs/ ./test/language/**/*.js --save-only-failed
