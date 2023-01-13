
# a library for unit tests

Largely inspired by [CMocka](https://cmocka.org/)

## Names

* Test Suite - a collection of tests to be performed
* Test Case - a single test; includes a testing function with assertions
* Mock - a function to be injected and return predefined value(s)

## Usage 

There's four components in testing:

* the function under test (FUT), to verify its behavior
* a mock function, that will be injected to the FUT, to use preset mock values,
* a test case function, to setup the injection, set mock values, call the FUT, assert results,
* a function that collects all the test cases and asks the framework to run them.


```c
    // this is the actual function we are unit testing (FUT)
    int lookup(char *name, func_ptr disk_reader) {
        if ((int err = (disk_reader)()) != 0)
            return err;
        
        // ... rest of logic
    }

    // this is a mock function to inject into the FUT
    int mock_disk_read(int sector, char *buffer, int len) {
        return mocked_value();
    }

    // this is the test case, that sets things up and asserts results
    int verify_lookup_returns_read_error() {
        mock_value(32);
        int err = lookup("test", mock_disk_read);
        assert(err == 32);
    }


    // this is a main function that prepares and executes the test suite
    int main() {
        unit_test_t tests[] = {
            unit_test(verify_lookup_returns_value_read),
        };
        run_tests(tests);
    }
```

## How it works

Maintains a set of data per each test case. The data is the following:

...

The macros pass in the __FILE__ and __function__ where the declarations were made,
therefore we know which method we are setting up for.

